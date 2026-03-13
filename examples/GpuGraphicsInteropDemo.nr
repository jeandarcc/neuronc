InteropLit is shader {
  tint as Color;

  Vertex method(position as Vector3) {
    return MVP * position;
  }

  Fragment method() {
    return tint;
  }
}

// Graphics v2 vertical slice:
// gpu work can still run in-frame, but render submission now uses typed
// mesh/material commands instead of dynamic Graphics.Draw(...).
Init method() {
  win is Window.Create(960, 540, "NPP GPU Graphics Interop Demo");
  mesh is Mesh.Load("examples/assets/triangle.obj");
  material is Material<InteropLit>();
  material.tint is Color(0.12, 0.82, 0.48, 1.0);

  canvas(win) {
    OnFrame {
      gpu {
        a is Tensor<float>.Random(3, 3);
        b is Tensor<float>.Random(3, 3);
        frameTensor is a + b;
      }

      cmd.Clear(Color(0.06, 0.06, 0.08, 1.0));
      cmd.DrawInstanced(mesh, material, 1);
    }
  }
}
