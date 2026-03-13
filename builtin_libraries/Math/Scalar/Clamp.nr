Clamp is method(value as float, minValue as float, maxValue as float) as float {
  return Max(minValue, Min(value, maxValue));
}

Saturate is method(value as float) as float {
  return Clamp(value, 0, 1);
}
