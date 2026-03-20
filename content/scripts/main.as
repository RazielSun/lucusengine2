void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2");

    RenderObject@ renderObject = g_renderer.EmplaceRenderObject();

    Mesh@ mesh = Mesh();
    renderObject.SetMesh(mesh);

    Material@ material = Material("triangle", 0);
    renderObject.SetMaterial(material);
}