Vector3Zero is method() as Vector3 {
  return Vector3(0, 0, 0);
}

Vector3One is method() as Vector3 {
  return Vector3(1, 1, 1);
}

Vector3Add is method(a as Vector3, b as Vector3) as Vector3 {
  return a + b;
}

Vector3Sub is method(a as Vector3, b as Vector3) as Vector3 {
  return a - b;
}

Vector3Scale is method(value as Vector3, scalar as float) as Vector3 {
  return value * scalar;
}

Vector3Div is method(value as Vector3, scalar as float) as Vector3 {
  return value / scalar;
}

Vector3Dot is method(a as Vector3, b as Vector3) as float {
  return Dot3(a.x, a.y, a.z, b.x, b.y, b.z);
}

Vector3LengthSquared is method(value as Vector3) as float {
  return Vector3Dot(value, value);
}

Vector3Length is method(value as Vector3) as float {
  return Sqrt(Vector3LengthSquared(value));
}

Vector3DistanceSquared is method(a as Vector3, b as Vector3) as float {
  delta is a - b;
  return Vector3LengthSquared(delta);
}

Vector3Distance is method(a as Vector3, b as Vector3) as float {
  return Sqrt(Vector3DistanceSquared(a, b));
}

Vector3Normalize is method(value as Vector3) as Vector3 {
  length is Vector3Length(value);
  if(length == 0) {
    return Vector3Zero();
  }
  return value / length;
}

Vector3Cross is method(a as Vector3, b as Vector3) as Vector3 {
  return Vector3(
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  );
}

Vector3Lerp is method(a as Vector3, b as Vector3, t as float) as Vector3 {
  return a + (b - a) * t;
}
