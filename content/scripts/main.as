void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2");

    RenderObject@ renderObject = g_renderer.EmplaceRenderObject();

    Camera@ camera = Camera();
    camera.SetPosition(0.0f, -0.5f, 5.0f);
    g_renderer.SetCamera(camera);

    Mesh@ mesh = Mesh();
    renderObject.SetMesh(mesh);

    // Material@ material = Material("triangle");
    Material@ material = Material("cube");
    material.SetUseUniformBuffers(true);
    renderObject.SetMaterial(material);
}