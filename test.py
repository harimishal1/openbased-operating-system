#!/usr/bin/env python3

"""
This is the OpenLSD testing library

This library orchestrates running tests against the OpenLSD kernel,
while performing assertions on the output produced. Call this file
with the --help flag for usage information.

This file is not typically invoked directly, but rather through one
of the make targets, such as `make test` or `make test-foo`.

Refer to `test/AUTHORING.md` for instructions on how to define and
add new tests.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from enum import Enum
import operator
import click
import json
from itertools import groupby, accumulate
from mashumaro.mixins.yaml import DataClassYAMLMixin
from mashumaro.types import Alias
from pathlib import Path
from rich import box
from rich.console import Console, ConsoleOptions, RenderResult, Group, RenderableType
from rich.live import Live
from rich.panel import Panel
from rich.rule import Rule
from rich.spinner import Spinner
from rich.text import Text
from typing import Any, Generator, Self, Annotated, Callable
import asyncio
import os
import re
import signal
import subprocess
import yaml

###############################################
#### General helper methods
###############################################

def make(*args: str) -> list[str]:
    """Create make command with the given arguments"""
    return [ "make", "-s", "--no-print-directory", *args, "INTERACTIVE=0" ]

def ansi_escape(text: str) -> str:
    """Escape all non-ANSI characters from QEMU output"""
    text = re.sub(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])', '', text)
    text = re.sub(r'\x1Bc', '', text)
    return text

async def run_command(
        *args: str,
        timeout: int | None = None,
        log: Log | None = None,
) -> tuple[int, str, str]:
    """Run a command while capturing the output and potentially streaming
    the output to the given log."""

    # Log the command to be executed
    if log is not None:
        log.title = f"[dim]$ {" ".join(args)}"

    process = await asyncio.create_subprocess_exec(
        *args,
        
        # Ensure all subprocesses can be killed upon timeout
        start_new_session=True,

        # Capture outputs
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,

        # Do not connect input to parent stdin, since that will
        # expose the user tty, thus allowing the command to configure
        # the terminal behaviour of the user calling this script.
        stdin=subprocess.PIPE,
    )

    # Helper method to process the stream. This behaves like `tee`,
    # sending any streamed data to both a string buffer, and to the
    # terminal streaming logs. This is aynchronous, to allow for
    # concurrent (not parallel!) processing of multiple commands.
    async def log_stream(stream: asyncio.StreamReader | None) -> str:
        if stream is None: return ""

        # Process the stream line-by-line, sending the output to
        # a string buffer and the logger
        result = ""
        while line := (await stream.readline()):
            line = line.decode()
            if log is not None:
                log.writeline(line)

            result += line

        return result

    # We use a task group here to allow the stream processing of
    # stdout and stderr, as well as the timeout, to happen
    # concurrently.
    async with asyncio.TaskGroup() as tg:
        stdout = tg.create_task(log_stream(process.stdout))
        stderr = tg.create_task(log_stream(process.stderr))

        # Wait for the process to finish, but cap the waiting
        # with a timeout. Once the timeout is hit, we kill the
        # process brute-force. In all cases, we assume the
        # stdout/stderr to be closed once the process has quit,
        # so then the tasks above will have completed as well.
        try:
            async with asyncio.timeout(timeout):
                await process.wait()
        
        except asyncio.exceptions.TimeoutError as _:
            # Kill the process group of the command to ensure it all stops
            os.killpg(os.getpgid(process.pid), signal.SIGKILL)

    return (process.returncode if process.returncode is not None else -1, stdout.result(), stderr.result())

###############################################
#### Terminal rendering helpers
###############################################

def dump_stdio(command: str | None, stdout: str, stderr: str) -> Generator[RenderableType]:
    """Method to dump stdout and stderr in case of runtime errors"""

    if command:
        yield f">> [red]{command}"

    yield Panel(
        ansi_escape(stdout).rstrip(),
        CUSTOM_BOX,
        title="[magenta]stdout",
        title_align="left",
    )

    yield Panel(
        ansi_escape(stderr).rstrip(),
        box.HORIZONTALS,
        title="[magenta]stderr",
        title_align="left",
    )

# Custom BOX layout for "rich", rendering only a top line
CUSTOM_BOX: box.Box = box.Box(
    " ── \n"
    "    \n"
    " ── \n"
    "    \n"
    " ── \n"
    " ── \n"
    "    \n"
    "    \n"
)

class Log:
    """Auto-scrolling display of a stream"""

    def __init__(self, maxlines: int, title: str | None = None) -> None:
        self.__maxlines = max(0, maxlines - 2)  # Subtract 2 lines for padding
        self.__lines: list[Text] = [Text("...", style="dim")]
        self.__title = title

    @property
    def title(self) -> str | None:
        return self.__title
    
    @title.setter
    def title(self, value: str | None):
        self.__title = value

    def writeline(self, line: str) -> None:
        self.__lines.append(Text(ansi_escape(line).rstrip()))
        self.__truncate()

    def write(self, text: Text) -> None:
        for line in text.split():
            self.__lines.append(line)
        self.__truncate()

    def __truncate(self) -> None:
        """Ensure the log display does not exceed the maximum number of lines"""

        # Truncate from the start (in a queue-fashion)
        self.__lines = self.__lines[-self.__maxlines:]

    def __rich_console__(self, console: Console, options: ConsoleOptions) -> RenderResult:
        if self.__maxlines == 0:
            return
        
        yield Panel(
            Group(*self.__lines),
            box.HORIZONTALS,
            title=self.title,
            title_align="left",
            style="dim",
        )
    
class Action:
    """Live reporting of running actions"""

    def __init__(self, message: str, console: Console, log_lines: int) -> None:
        self.__console = console
        self.__log_lines = log_lines
        self.__spinner = Spinner("dots", Text.from_markup(message))
        self.__log: Log | None = None
    
    @property
    def log(self) -> Log:
        if self.__log is None:
            raise ValueError("No log available")
        return self.__log
    
    def start(self):
        self.__log = Log(self.__log_lines)
    
    def done(self, message: str) -> str | None:
        title = self.log.title

        self.__console.print(Text.from_markup(message))
        self.__log = None
        self.__spinner = None

        return title

    def success(self, message: str) -> str | None:
        return self.done(f"[green]✓[/] {message}")

    def fail(self, message: str) -> str | None:
        return self.done(f"[red]✗[/] {message}")

    def __rich_console__(self, console: Console, options: ConsoleOptions) -> RenderResult:
        if self.__spinner: yield self.__spinner
        if self.__log: yield self.__log

class ActionRunner:
    """Wrapper for multiple actions"""

    def __init__(self, console: Console, log_lines: int = 0) -> None:
        self.__console = console
        self.__log_lines = log_lines
        self.__actions: list[Action] = []

    @property
    def console(self) -> Console:
        return self.__console

    def run(self, message: str, log_lines: int | None = None) -> Action:
        action = Action(message, self.__console, log_lines if log_lines != None else self.__log_lines)
        self.__actions.append(action)
        return action
    
    def __rich_console__(self, console: Console, options: ConsoleOptions) -> RenderResult:
        yield from self.__actions
        yield "" # This is a hack to workaround a bug in Live rendering

###############################################
#### Test definitions and handling
###############################################
class TestStatus(Enum):
    CRASHED = 1
    TIMEOUT = 2
    FAILED = 3
    SUCCESS = 4

@dataclass
class TestReport:
    """Report of test results"""

    test: Test

    # Execution results
    output: str
    error: str
    result: TestStatus

    # Output results
    lines: list[str] = field(default_factory=list[str])
    good_lines: set[int] = field(default_factory=set[int])
    bad_lines: set[int] = field(default_factory=set[int])
    missing_lines: set[str] = field(default_factory=set[str])
    absent_lines: set[str] = field(default_factory=set[str])

    def __highlight_lines(self, lines: set[int], color: str, context: int = 1) -> Generator[RenderableType]:
        """Method to highlight selected lines from the stdout of the test"""

        if len(lines) == 0:
            return

        # Roughly, the logic here is as follows: we keep track of the
        # most-recently printed line. Then for each line to highlight,
        # we check whether its context window would overlap or touch
        # the last printed line. If that is not the case, we print a
        # section break.
        last_line = 0
        for line in sorted(lines):
            # We actually print the "tail end" of the context window
            # of the previous highlighted line at the start of a new
            # loop iteration, for... reasons I can't remember.

            # Print "tail end" of previous context window
            if last_line > 0:
                for i in range(last_line + 1, min(last_line + context + 1, line - context)):
                    yield f"[dim]{i}. {self.lines[i]}"
                    last_line = i

            # Print optional section break
            if line - context > last_line + 1:
                yield Rule("···", align="center", style="dim")

            # Print the leading context window of the next highligted line
            for i in range(max(line - context, last_line + 1), line):
                yield f"[dim]{i}. {self.lines[i]}"
                last_line = i

            # Print the highlighted line
            yield f"{line}. [{color}]{self.lines[line]}"
            last_line = line

        # Print the tail end of the last context window
        for i in range(min(last_line + 1, len(self.lines) + 1), min(last_line + context + 1, len(self.lines))):
            yield f"[dim]{i}. {self.lines[i]}"

        # End the output with either "EOF" or another section break
        if len(self.lines) - context > last_line + 1:
            yield Rule("···", align="center", style="dim")
        else:
            yield f"[dim]EOF"

    def status_symbol(self) -> str:
        """Returns a status symbol"""

        if self.result == TestStatus.SUCCESS:
            return "[green]✓[/]"
        else:
            return "[red]✗[/]"

    def status_message(self) -> str:
        """Returns a short status message"""
        
        # Handle error cases
        if self.result == TestStatus.CRASHED:
            return "Crashed"

        if self.result == TestStatus.TIMEOUT:
            return "Timeout"

        # Print test output
        if self.result == TestStatus.FAILED:
            return "Incorrect Output"
        
        return "Passed"

    def print_error(self, context: int) -> Generator[RenderableType]:
        """Error-only test result reporting"""

        title = f"Error Report for \"{self.test.friendly_name}\" ([cyan]{self.test.test_lab}_{self.test.test_name}[/])"

        # Handle error cases
        if self.result == TestStatus.CRASHED:
            yield from dump_stdio(None, self.output, self.error)
            return

        if self.result == TestStatus.TIMEOUT:
            yield from dump_stdio(None, self.output, self.error)
            return
        
        # Success case
        if self.result == TestStatus.SUCCESS:
            return

        # Print test output
        panels: list[Panel] = []
        if len(self.missing_lines) > 0:
            panels.append(Panel(
                title="Missing lines",
                title_align="left",
                box=CUSTOM_BOX,
                renderable=Group(*[f"[red]{missing}" for missing in self.missing_lines]),
                padding=(0,1,0,3),
            ))

        if len(self.bad_lines) > 0:
            panels.append(Panel(
                title="Incorrect lines that should not be present",
                title_align="left",
                box=CUSTOM_BOX,
                renderable=Group(*self.__highlight_lines(self.bad_lines, "red", context)),
            ))

        yield ""
        yield Panel(
            Group(*panels),
            title=title,
            title_align="left",
            box=box.ROUNDED,
            expand=False,
            padding=(1,1,0,1)
        )

    def print_full(self, context: int) -> Generator[RenderableType]:
        """Full test result reporting"""

        title = f"Detailed Report for \"{self.test.friendly_name}\" ([cyan]{self.test.test_lab}_{self.test.test_name}[/])"

        # Handle error cases
        if self.result == TestStatus.CRASHED:
            yield from dump_stdio(None, self.output, self.error)
            return

        if self.result == TestStatus.TIMEOUT:
            yield from dump_stdio(None, self.output, self.error)
            return

        # Print test output
        panels: list[Panel] = []
        if len(self.good_lines) > 0:
            panels.append(Panel(
                title="Correct output lines found",
                title_align="left",
                box=CUSTOM_BOX,
                renderable=Group(*self.__highlight_lines(self.good_lines, "green", context)),
            ))

        if len(self.absent_lines) > 0:
            panels.append(Panel(
                title="Correctly absent lines",
                title_align="left",
                box=CUSTOM_BOX,
                renderable=Group(*[f"[green]{absent}" for absent in self.absent_lines]),
                padding=(0,1,0,3),
            ))

        if len(self.missing_lines) > 0:
            panels.append(Panel(
                title="Missing lines",
                title_align="left",
                box=CUSTOM_BOX,
                renderable=Group(*[f"[red]{missing}" for missing in self.missing_lines]),
                padding=(0,1,0,3),
            ))

        if len(self.bad_lines) > 0:
            panels.append(Panel(
                title="Incorrect lines that should not be present",
                title_align="left",
                box=CUSTOM_BOX,
                renderable=Group(*self.__highlight_lines(self.bad_lines, "red", context)),
            ))

        yield ""
        yield Panel(
            Group(*panels),
            title=title,
            title_align="left",
            box=box.ROUNDED,
            expand=False,
            padding=(1,1,0,1)
        )

@dataclass
class Test(DataClassYAMLMixin):
    """Description of an OpenLSD test"""

    # Test information
    friendly_name: Annotated[str, Alias("name")]
    description: str = ""
    test_name: str = ""
    test_lab: str = ""
    has_kernel: bool = False
    has_user: bool = False
    should_panic: bool = False
    qemu: str = ""
    timeout: int = 30
    bonus: str = ""
    required: bool = False

    # Matching information
    matchlines: list[str] = field(default_factory=list[str])
    nomatchlines: list[str] = field(default_factory=list[str])
    regexlines: list[str] = field(default_factory=list[str])
    noregexlines: list[str] = field(default_factory=list[str])

    async def __run(self, log: Log, do_build: bool, bonus: str) -> TestReport:
        """Run the current test through make"""

        (exitcode, stdout, stderr) = await run_command(
            *make(
                "run" if do_build else "exec",
                f"TEST={self.test_lab}_{self.test_name}",
                "-j1",
                f"QEMUEXTRA={self.qemu}",
                f"BONUS={bonus}"
            ),
            timeout=self.timeout,
            log=log
        )

        match exitcode:
            case 0: result = TestStatus.SUCCESS
            case -9: result = TestStatus.TIMEOUT
            case _: result = TestStatus.CRASHED

        return TestReport(
            test = self,
            output = stdout,
            error = stderr,
            result = result,
        )

    def __assert_lines(self, report: TestReport) -> TestReport:
        """Check that the test output matches the test specification"""

        # No point in checking output if execution failed
        if report.result != TestStatus.SUCCESS:
            return report

        # Perform all test assertions
        report.lines = ansi_escape(report.output).splitlines()
        remaining = report.lines.copy()

        for line in self.matchlines:
            matches = [ i for i, s in enumerate(remaining) if s == line ]
            if not matches:
                report.missing_lines.add(line)
            else:
                remaining[matches[0]] = ""
                report.good_lines.add(matches[0])

        for regex in self.regexlines:
            matches = [ i for i, s in enumerate(remaining) if re.search(regex, s) ]
            if not matches:
                report.missing_lines.add(f"/{regex}/")
            else:
                remaining[matches[0]] = ""
                report.good_lines.add(matches[0])

        for line in self.nomatchlines:
            matches = [ i for i, s in enumerate(remaining) if s == line ]
            if not matches:
                report.absent_lines.add(line)
            else:
                report.bad_lines = report.bad_lines.union(matches)

        for regex in self.noregexlines:
            matches = [ i for i, s in enumerate(remaining) if re.search(regex, s) ]
            if not matches:
                report.absent_lines.add(f"/{regex}/")
            else:
                report.bad_lines = report.bad_lines.union(matches)

        if report.bad_lines or report.missing_lines:
            report.result = TestStatus.FAILED

        return report

    async def test(self, log: Log, do_build: bool = True, bonus: str = "") -> TestReport:
        """Run the current test and check the output"""
        
        report = await self.__run(log, do_build, bonus)
        report = self.__assert_lines(report)

        return report

    @classmethod
    def load(cls, path: Path) -> list[Self]:
        """Load a test from a specification file (spec.yml)"""

        # Read test specs from file; with inheritance of settings
        docs : list[dict[Any, Any]] = list(yaml.safe_load_all(path.read_text()))
    
        # Merge successive YAML documents
        docs = list(accumulate(docs, operator.or_))

        # Convert to Test instances
        specs = [ cls.from_dict(doc) for doc in docs ] # type: ignore

        # Append friendly name of later tests
        for spec in specs[1:]:
            spec.friendly_name = specs[0].friendly_name + " " + spec.friendly_name

        for spec in specs:
            # Test metadata is taken from its path
            spec.test_name = path.parent.name
            spec.test_lab = path.parent.parent.name

            # Test features are detected from path as well
            spec.has_kernel = (path.parent / 'kernel.c').exists()
            spec.has_user = (path.parent / 'user.c').exists()

            # Always require test preamble and completion
            spec.matchlines.append(f"[TESTS] Running test '{spec.test_lab}_{spec.test_name}'")
            spec.matchlines.append(f"Finished kernel execution")

            # Depending on test type, we should add particular matches
            # or nomatches
            test_pass_line = f"[TESTS] Test '{spec.test_lab}_{spec.test_name}' passed!"
            panic_regex = r"(kernel|user) panic"

            if (spec.has_kernel and spec.should_panic):
                spec.nomatchlines.append(test_pass_line)
                
                spec.regexlines.append(panic_regex)

            elif (spec.has_kernel):
                spec.matchlines.append(test_pass_line)
                spec.noregexlines.append(panic_regex)

            else:
                spec.noregexlines.append(panic_regex)

        return specs

    @classmethod
    def load_all(cls, path: Path, variants: bool = True) -> list[Self]:
        """Load all tests in the current directory (recursively) into a list"""

        paths = path.glob("**/spec.yml")
        if variants:
            return [ test for path in paths for test in cls.load(path) ]
        else:
            return [ cls.load(path)[0] for path in paths ]


async def build_kernel(runner: ActionRunner, cores: int = 0, lab: int | None = None, bonus: str = "") -> bool:
    """Run a full clean kernel build to prepare for any tests"""

    action = runner.run("Cleaning build...")
    action.start()

    (exitcode, stdout, stderr) = await run_command(
        *make("clean"),
        timeout=120,
        log=action.log
    )
    
    if exitcode != 0:
        command = action.fail(
            "Clean timeout!" if exitcode == -9 else
                f"Clean error! Make exited with code {exitcode}"
        )
        runner.console.print(*dump_stdio(command, stdout, stderr))
        return False
    
    action.success(f"Cleaned build")
    action = runner.run("Building kernel...")
    action.start()

    (exitcode, stdout, stderr) = await run_command(
        *make(
            "all",
            *([f"LAB={lab}"] if lab != None else []),
            *([f"-j{cores}"] if cores > 0 else []),
            f"BONUS={bonus}",
        ),
        timeout=300,
        log=action.log
    )

    if exitcode != 0:
        command = action.fail(
            "Build timeout!" if exitcode == -9 else
                f"Build error! Make exited with code {exitcode}"
        )
        runner.console.print(*dump_stdio(command, stdout, stderr))
        return False

    action.success(f"Built kernel")

    return True

async def run_tests(tests: list[Test], parallel: int, bonus: str, runner: ActionRunner) -> list[TestReport]:
    """Run all tests in the list and report on the progress and output. This assumes the build to have been completed"""

    sem = asyncio.Semaphore(parallel)

    async def run_test(test: Test) -> TestReport:
        async with sem:
            action = runner.run(f"Test '{test.friendly_name}'", int(runner.console.size.height * 0.8 / parallel))
            action.start()
            report = await test.test(action.log, do_build=False, bonus=bonus)

        action.done(f"{report.status_symbol()} Test '{test.friendly_name}' - {report.status_message()}")

        return report

    async with asyncio.TaskGroup() as tg:
        test_tasks = [ tg.create_task(run_test(test)) for test in tests ]

    return [ task.result() for task in test_tasks ]

def list_tests(ctx: click.Context, param: click.Parameter, value: bool):
    """Find all tests and write a file with metadata to disk, for editor integrations"""
    
    if not value or ctx.resilient_parsing:
        return
    
    lab_selector: Callable[[Test], str] = lambda t : t.test_lab
    
    tests = Test.load_all(Path(f"test"), variants=False)
    grouped = { 
        k.removeprefix("lab") : [ {
            "name": test.test_name,
            "lab": test.test_lab.removeprefix("lab"),
            "friendly_name": f"Lab {test.test_lab.removeprefix("lab")} - {test.friendly_name}",
            "description": test.description,
        } for test in v ]
        for k, v in groupby(sorted(tests, key = lab_selector), lab_selector)
    }

    Path(".vscode").mkdir(parents=True, exist_ok=True)

    with open(".vscode/labs.json", "w", encoding="utf-8") as file:
        json.dump(list(grouped.keys()), file)

    for lab in grouped.keys():
        with open(f".vscode/lab{lab}.json", "w", encoding="utf-8") as file:
            json.dump(grouped.get(lab), file)

    ctx.exit()

@click.command("lab")
@click.argument("lab", type=click.IntRange(1,7), required=True)
@click.argument("tests", type=click.STRING, nargs=-1)
@click.option("--clean/--no-clean", default=True, help="Perform a clean build before testing.", show_default=True)
@click.option("--build-cores", default=0, help="Number of cores to use for building. 0 = number of system cores", show_default=True)
@click.option("--parallel", default=1, help="Number of tests to run in parallel.", show_default=True)
@click.option("--context", default=1, help="For test reports, the number of lines of code of context to show.", show_default=True)
@click.option("-v", "--verbose", is_flag=True, default=False, help="Show detailed test reports for all tests.")
@click.option("--error-only", is_flag=True, default=False, help="Only display error output.")
@click.option("--stderr", is_flag=True, default=False, help="Print all QEMU output to stderr.")
@click.option("--bonus", default="", help="Which bonus features to test, iteratively")
@click.option("--basic", is_flag=True, default=False, help="Only run basic tests for the current lab")
#
@click.option("--list-tests", is_flag=True, default=False, expose_value=False, is_eager=True, callback=list_tests, help="Write all available tests to a file for editor integrations")
def cli(
    lab: int,
    tests: tuple[str],
    clean: bool,
    build_cores: int,
    parallel: int,
    context: int,
    verbose: bool,
    error_only: bool,
    stderr: bool,
    bonus: str,
    basic: bool,
):
    """
    Runs the selected TESTS from lab number LAB. When no TESTS
    are specified, all tests from the selected lab are executed.
    When a single TEST is specified, a detailed test output will
    be shown by default.
    """

    # Process input
    bonuses = bonus.split()
    if not bonuses:
        bonuses.append("")

    # Set up interface components for nice output
    console = Console(highlight=False)
    runner = ActionRunner(console, int(console.size.height / 2))
    live = Live(runner, transient=True, refresh_per_second=20, console=console)

    # Gather tests
    if len(tests) == 0:
        specs = Test.load_all(Path(f"test/lab{lab}/"))

    else:
        specs: list[Test] = []
        for test in tests:
            test_path = Path(f"test/lab{lab}/{test}/spec.yml")
            if not test_path.exists():
                console.print(f"Error: test {test} for lab {lab} does not exist!")
                exit(1)

            specs += Test.load(test_path)

    # Filter basic tests
    if basic:
        specs = [ spec for spec in specs if spec.required ]

    if not specs:
        console.print(f"No tests matched the selection critera!")
        exit(1)

    # Start execution for each bonus
    success = True
    for bonus in bonuses:
        with live:
            if bonus == "NONE":
                console.print(f"No bonus to test...")
                exit(1)

            if bonus != "":
                console.print(f"Testing bonus feature {bonus}...")

            if basic:
                console.print(f"Running only basic tests...")

            # Filter specs for current bonus:
            #   When bonus == "", include all non-bonus tests
            #   When bonus == "foo", include all non-bonus tests AND "foo" bonus tests
            bonus_specs = [ spec for spec in specs if spec.bonus == "" or spec.bonus == bonus ]

            if not bonus_specs:
                console.print(f"No tests matched the selection criteria!")
                continue

            # Run clean and build if needed
            if clean:
                result = asyncio.run(build_kernel(runner, build_cores, lab, bonus))
                if result == False: exit(1)
            
            # Run test specs
            reports = asyncio.run(run_tests(bonus_specs, parallel, bonus, runner))

        # Report on results
        # Single & EONLY & FAIL -> Error
        # Single -> Full
        # Multiple & FAIL -> Error
        # Multiple & verbose -> Full
        for report in reports:
            success = success and (report.result == TestStatus.SUCCESS)

            if verbose or (len(reports) == 1 and not error_only):
                console.print(*report.print_full(context))
            elif report.result != TestStatus.SUCCESS:
                console.print(*report.print_error(context))

            # Dump all stdio to stderr
            if stderr:
                error_console = Console(stderr=True)
                error_console.print(*dump_stdio(None, report.output, report.error))

    if not success:
        exit(1)

if __name__ == '__main__':
    cli()
