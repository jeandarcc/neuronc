Node is class {
    value is 0 as int;
}

Init is method() {
    x is 42;
    pointerValue is address of x;
    nodeObject is Node();
    nodeObject.value is 100;

    Print(value of pointerValue);
    Print(nodeObject.value);
}
