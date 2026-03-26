void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2");

    RenderObject@ renderObject = g_renderer.EmplaceRenderObject();

    Camera@ camera = Camera();
    camera.SetPosition(0.0f, -0.5f, 5.0f);
    g_renderer.SetCamera(camera);

    bool drawSimpleTriangle = false; // Change to false to draw a cube

    if (drawSimpleTriangle)
    {
        Mesh@ triangle = Mesh();
        triangle.SetDrawCount(3);
        renderObject.SetMesh(triangle);

        Material@ triangle_mat = Material("triangle");
        renderObject.SetMaterial(triangle_mat);
    }
    else
    {
        Mesh@ cube = Mesh();
        cube.SetDrawCount(36);
        renderObject.SetMesh(cube);

        Material@ cube_mat = Material("cube");
        cube_mat.SetUseUniformBuffers(true);
        renderObject.SetMaterial(cube_mat);
    }
}