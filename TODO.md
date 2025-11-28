## TODO

## Native Calls

Assembly info will include foreign C function names and signatures soon.
Create a hashmap or equivalent to store (func-name, id) pairs, create a syscall
that takes a VM string pointer, uses it to get the ID and calls the function at that id.
Or get rid of that ID stuff alltogether.
