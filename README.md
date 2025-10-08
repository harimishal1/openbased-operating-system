## Implemented bonus features

### Invalid Free Detection

Feature flag: INVALID_FREE_DETECTION

Freeing from the middle of a higher order chunk is considered to be an invalid free. As such, the protection for it is to mark all the pages that part of a higher order with an invalid order value. This is marked upon allocation and checked upon freeing. Thus we can ensure that higher order chunks are never freed. 

### Double Free Detection

Feature flag: DOUBLE_FREE_DETECTION

Two asserts inside the free function check along with a check whether the page currently being freed has the pp_free attribute set or not, ensures that double free doesn't occur. 

### Enabling The Tests

When the two define lines at the top of buddy.c are enabled, the bonus features are activated. When commented out, they fail. 
