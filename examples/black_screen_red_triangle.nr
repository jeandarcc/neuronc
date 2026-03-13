BasicLit is shader {
  tint as Color;

  Vertex method(position as Vector3) {
    return MVP * position;
  }

  Fragment method() {
    return tint;
  }
}

Init method() {
  mainWindow is Window.Create(800, 600, "Graphics V2 Triangle");
  triangleMesh is Mesh.Load("examples/assets/triangle.obj");
  triangleMaterial is Material<BasicLit>();
  triangleMaterial.tint is Color(1.0, 0.15, 0.10, 1.0);

  canvas(mainWindow) {
    OnFrame {
      cmd.Clear(Color(0.08, 0.08, 0.10, 1.0));
      cmd.DrawIndexed(triangleMesh, triangleMaterial);
    }
  }
}
