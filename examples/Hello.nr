globalValue is 100;

Init is method() {
    Print("Global value before:");
    Print(globalValue);

    globalValue is globalValue + 50;
    copiedValue is another globalValue; // not a pointer, just a copy

    Print("Global value after:");
    Print(globalValue);
    Print(copiedValue);
}
