module System;

Init is method() {
    try {
        throw "try-catch smoke";
    } catch(errorValue) {
        Print(errorValue);
    } finally {
        Print("finally block");
    }
}
