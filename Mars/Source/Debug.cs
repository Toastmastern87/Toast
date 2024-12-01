using System;

using Toast;

class Debug : Entity
{
    private bool keyPressed;

    void OnCreate()
    {
        keyPressed = false;
        Toast.Console.LogCritical("OnCreate");
    }

    void OnClick()
    {
    }

    void OnUpdate(float ts)
    {
        if (Input.IsKeyPressed(KeyCode.C) && !keyPressed)
        {
            keyPressed = true;

            Toast.Console.LogCritical("Activating Colliders");

            Scene.SetRenderColliders(!Scene.GetRenderColliders());
        }
        else if(!Input.IsKeyPressed(KeyCode.C))
            keyPressed = false;
    }
}