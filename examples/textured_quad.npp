TexturedQuad is shader {
  tint as Color;
  albedo as Texture2D;
  linearSampler as Sampler;

  Vertex method(position as Vector3, uv as Vector2) {
    pass uv;
    return MVP * position;
  }

  Fragment method(uv as Vector2) {
    return albedo.Sample(linearSampler, uv) * tint;
  }
}

Init method() {
  mainWindow is Window.Create(960, 640, "Graphics V2 Textured Quad");
  quadMesh is Mesh.Load("examples/assets/quad.obj");
  quadTexture is Texture2D.Load("examples/assets/checker.png");
  quadSampler is Sampler.Create();
  quadMaterial is Material<TexturedQuad>();
  quadMaterial.tint is Color(1.0, 1.0, 1.0, 1.0);
  quadMaterial.albedo is quadTexture;
  quadMaterial.linearSampler is quadSampler;

  canvas(mainWindow) {
    OnFrame {
      cmd.Clear(Color(0.07, 0.08, 0.10, 1.0));
      cmd.DrawIndexed(quadMesh, quadMaterial);
    }
  }
}
