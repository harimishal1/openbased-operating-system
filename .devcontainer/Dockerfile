# We use an Ubuntu 24.04 baseline image
FROM --platform=linux/amd64 ubuntu:24.04

# Configure locale to keep Perl and other apps happy
ENV LANG=en_GB.UTF-8
ENV LANGUAGE=en_GB.UTF-8
ENV LC_ALL=en_GB.UTF-8
RUN echo "en_GB.UTF-8 UTF-8" > /etc/locale.gen

# Update base packages
RUN apt-get update && apt-get upgrade -y

# Install necessary packages for OpenLSD
RUN apt-get update && apt-get install -y \
	# System packages
	sudo curl git wget htop gnupg2 nano locales \
	# OpenLSD dependencies
	bear build-essential make python3 python-is-python3 python3-pip qemu-system unifdef \
	# LLVM
	clang clangd llvm lld gdb clang-format

# Install Python packages
RUN pip install click==8.2.1 mashumaro==3.16 rich==14.0.0 --break-system-packages

# Install further dependencies
RUN wget https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 -O /usr/local/bin/yq &&\
    chmod +x /usr/local/bin/yq

# Configure parallel compilation
RUN echo "export MAKEFLAGS=\"-j$(nproc)\"" > /etc/profile.d/makeflags.sh

# Configure ubuntu user
RUN mkdir -p /etc/ssh/sshd_config.d/ && \
	echo "PermitEmptyPasswords yes" > /etc/ssh/sshd_config.d/empty_pass.conf && \
	echo "ubuntu ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers && \
	passwd -d ubuntu
USER ubuntu
