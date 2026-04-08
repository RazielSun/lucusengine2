void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2 - Triangle Test");

    Scene@ scene = Scene();
    g_renderer.SetCurrentScene(scene);

    Camera@ camera = Camera();
    camera.SetPosition(Vec3(0.f, 0.f, 5.f));
    scene.SetCamera(camera);

    RenderObject@ object = scene.NewObject();

    Mesh@ triangle = Mesh::triangle();

    object.SetMesh(triangle);

    Material@ triangle_mat = Material("nounb_simple");
    triangle_mat.SetUseVertexIndexBuffers(true);

    object.SetMaterial(triangle_mat);
}