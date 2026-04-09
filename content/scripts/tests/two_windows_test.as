void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2 - Two Windows Test");
    g_window_manager.CreateWindow(800, 600, "Lucus Engine 2 - Advanced");

    Scene@ scene = Scene::load("content/assets/geometry/scene.gltf");
    g_renderer.SetCurrentScene(scene);
}