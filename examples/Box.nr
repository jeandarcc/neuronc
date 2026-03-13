Box is class<T> {
    value is T;
}

Init is method() {
    boxValue is Box<int>();
    boxValue.value is 123;
    Print(boxValue.value);
}
