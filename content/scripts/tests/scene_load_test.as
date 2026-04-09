void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2 - Scene Load Test");

    Scene@ scene = Scene::load("content/assets/geometry/scene.gltf");
    g_renderer.SetCurrentScene(scene);
}