void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2");

    Material@ cube_mat = Material("simple_lit");
    cube_mat.SetUseFrameUniformBuffer(true);
    cube_mat.SetUseObjectUniformBuffer(true);
    cube_mat.SetUseVertexIndexBuffers(true);

    Scene@ scene = Scene::load_with_material("content/assets/geometry/scene.gltf", cube_mat);

    g_renderer.SetCurrentScene(scene);
}