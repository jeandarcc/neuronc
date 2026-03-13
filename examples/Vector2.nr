module System;
module Math;

Vector2 is public class {
    x is 0.0 as float;
    y is 0.0 as float;

    constructor is public method(x as float, y as float) {
        this.x is x;
        this.y is y;
    }

    Length is public method() as float {
        return Math.sqrt(this.x * this.x + this.y * this.y);
    }
}

Init is method() {
    vectorObject is Vector2(3.0, 4.0);
    lengthValue is vectorObject.Length();
    Print(lengthValue);
}
