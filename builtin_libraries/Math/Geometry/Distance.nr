Distance2 is method(ax as float, ay as float, bx as float, by as float) as float {
  dx is bx - ax;
  dy is by - ay;
  return Length2(dx, dy);
}

Distance3 is method(ax as float, ay as float, az as float, bx as float, by as float, bz as float) as float {
  dx is bx - ax;
  dy is by - ay;
  dz is bz - az;
  return Length3(dx, dy, dz);
}

