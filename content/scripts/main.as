void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2");
    // g_window_manager.CreateWindow(800, 600, "Lucus Engine 2 - Advanced");

    // if (false)
    {
        Scene@ scene = Scene::load("content/assets/scene.gltf");
        g_renderer.SetCurrentScene(scene);
    }

    if (false)
    {
        Scene@ scene = Scene();
        g_renderer.SetCurrentScene(scene);

        Camera@ camera = Camera();
        camera.SetPosition(Vec3(0.f, 0.f, 5.f));
        scene.SetCamera(camera);

        bool drawSimpleTriangle = false; // Change to false to draw a cube

        if (drawSimpleTriangle)
        {
            RenderObject@ object = scene.NewObject();

            // Mesh@ triangle = Mesh("", 3); // hardcoded triangle
            Mesh@ triangle = Mesh::triangle(); // non hardcoded triangle

            object.SetMesh(triangle);

            // Material@ triangle_mat = Material("triangle"); // hardcoded triangle
            Material@ triangle_mat = Material("nounb_simple");
            triangle_mat.SetUseVertexIndexBuffers(true); // non hardcoded triangle

            object.SetMaterial(triangle_mat);
        }
        else
        {
            // Mesh@ cube = Mesh("", 36); // hardcoded cube
            Mesh@ cube = Mesh::load("content/assets/cube.gltf");
            // Mesh@ cube = Mesh::cube();

            // Material@ cube_mat = Material("cube"); // hardcoded cube
            Material@ cube_mat = Material("simple");
            cube_mat.SetUseUniformBuffers(true);
            cube_mat.SetUseVertexIndexBuffers(true); // non hardcoded cube

            RenderObject@ cube_obj1 = scene.NewObject();
            cube_obj1.SetPosition(Vec3(-0.8f, 0.2f, 0.f));
            cube_obj1.SetRotationEuler(Vec3(0.f, 0.f, 0.f));
            cube_obj1.SetMesh(cube);
            cube_obj1.SetMaterial(cube_mat);

            RenderObject@ cube_obj2 = scene.NewObject();
            cube_obj2.SetPosition(Vec3(0.f, 0.f, 0.f));
            cube_obj2.SetRotationEuler(Vec3(0.f, 10.f, 45.f));
            cube_obj2.SetMesh(cube);
            cube_obj2.SetMaterial(cube_mat);

            RenderObject@ cube_obj3 = scene.NewObject();
            cube_obj3.SetPosition(Vec3(0.8f, 0.2f, 0.f));
            cube_obj3.SetRotationEuler(Vec3(90.f, 0.f, 0.f));
            cube_obj3.SetMesh(cube);
            cube_obj3.SetMaterial(cube_mat);
        }
    }
    
}