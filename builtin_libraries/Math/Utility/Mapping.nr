Remap is method(value as float, inMin as float, inMax as float, outMin as float, outMax as float) as float {
  t is InverseLerp(inMin, inMax, value);
  return Lerp(outMin, outMax, t);
}

SmoothStep is method(edge0 as float, edge1 as float, value as float) as float {
  t is Saturate((value - edge0) / (edge1 - edge0));
  return t * t * (3 - 2 * t);
}
