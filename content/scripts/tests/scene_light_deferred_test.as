void main()
{
    g_renderer.SetRenderMode(RenderMode::DEFERRED);
    
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2");

    Material@ test_mat = Material("simple_lit");
    test_mat.SetUseFrameUniformBuffer(true);
    test_mat.SetUseObjectUniformBuffer(true);
    test_mat.SetUseVertexIndexBuffers(true);

    Scene@ scene = Scene::load_with_material("content/assets/geometry/scene.gltf", test_mat);

    g_renderer.SetCurrentScene(scene);
}