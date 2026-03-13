Init method() {
  window is Window.Create(1280, 720, "Scene2D");
  scene is Scene.Create();

  cameraEntity is scene.CreateEntity("Camera");
  cameraTransform is cameraEntity.GetTransform();
  cameraTransform.SetPosition(Vector3(0.0, 0.0, 5.0));
  camera is cameraEntity.AddCamera2D();
  camera.SetPrimary(true);
  camera.SetZoom(1.25);

  spriteEntity is scene.CreateEntity("Sprite");
  spriteTransform is spriteEntity.GetTransform();
  spriteTransform.SetPosition(Vector3(-1.0, 0.0, 0.0));
  spriteTransform.SetScale(Vector3(2.0, 2.0, 1.0));
  sprite is spriteEntity.AddSpriteRenderer2D();
  sprite.SetTexture(Texture2D.Load("examples/assets/checker.png"));
  sprite.SetSize(Vector2(2.0, 2.0));
  sprite.SetPivot(Vector2(0.5, 0.5));
  sprite.SetSortingLayer(1);

  shapeEntity is scene.CreateEntity("Shape");
  shapeTransform is shapeEntity.GetTransform();
  shapeTransform.SetPosition(Vector3(1.5, 0.0, 0.0));
  shape is shapeEntity.AddShapeRenderer2D();
  shape.SetCircle(0.8, 32);
  shape.SetColor(Color(0.20, 0.85, 0.45, 1.0));
  shape.SetFilled(true);

  renderer is Renderer2D.Create();
  renderer.SetClearColor(Color(0.08, 0.08, 0.10, 1.0));
  renderer.SetCamera(camera);

  canvas(window) {
    OnFrame {
      renderer.Render(scene);
    }
  }
}
