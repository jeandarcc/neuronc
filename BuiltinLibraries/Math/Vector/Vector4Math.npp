Vector4Zero is method() as Vector4 {
  return Vector4(0, 0, 0, 0);
}

Vector4One is method() as Vector4 {
  return Vector4(1, 1, 1, 1);
}

Vector4Add is method(a as Vector4, b as Vector4) as Vector4 {
  return a + b;
}

Vector4Sub is method(a as Vector4, b as Vector4) as Vector4 {
  return a - b;
}

Vector4Scale is method(value as Vector4, scalar as float) as Vector4 {
  return value * scalar;
}

Vector4Div is method(value as Vector4, scalar as float) as Vector4 {
  return value / scalar;
}

Vector4Dot is method(a as Vector4, b as Vector4) as float {
  return Dot4(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
}

Vector4LengthSquared is method(value as Vector4) as float {
  return Vector4Dot(value, value);
}

Vector4Length is method(value as Vector4) as float {
  return Sqrt(Vector4LengthSquared(value));
}

Vector4Normalize is method(value as Vector4) as Vector4 {
  length is Vector4Length(value);
  if(length == 0) {
    return Vector4Zero();
  }
  return value / length;
}

Vector4Lerp is method(a as Vector4, b as Vector4, t as float) as Vector4 {
  return a + (b - a) * t;
}
