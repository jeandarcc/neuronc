Vector2Zero is method() as Vector2 {
  return Vector2(0, 0);
}

Vector2One is method() as Vector2 {
  return Vector2(1, 1);
}

Vector2Add is method(a as Vector2, b as Vector2) as Vector2 {
  return a + b;
}

Vector2Sub is method(a as Vector2, b as Vector2) as Vector2 {
  return a - b;
}

Vector2Scale is method(value as Vector2, scalar as float) as Vector2 {
  return value * scalar;
}

Vector2Div is method(value as Vector2, scalar as float) as Vector2 {
  return value / scalar;
}

Vector2Dot is method(a as Vector2, b as Vector2) as float {
  return Dot2(a.x, a.y, b.x, b.y);
}

Vector2LengthSquared is method(value as Vector2) as float {
  return Vector2Dot(value, value);
}

Vector2Length is method(value as Vector2) as float {
  return Sqrt(Vector2LengthSquared(value));
}

Vector2DistanceSquared is method(a as Vector2, b as Vector2) as float {
  delta is a - b;
  return Vector2LengthSquared(delta);
}

Vector2Distance is method(a as Vector2, b as Vector2) as float {
  return Sqrt(Vector2DistanceSquared(a, b));
}

Vector2Normalize is method(value as Vector2) as Vector2 {
  length is Vector2Length(value);
  if(length == 0) {
    return Vector2Zero();
  }
  return value / length;
}

Vector2Lerp is method(a as Vector2, b as Vector2, t as float) as Vector2 {
  return a + (b - a) * t;
}
