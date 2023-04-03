using System;

using Toast;

class Debug : Entity
{
    private bool keyPressed;

    void OnCreate()
    {
        keyPressed = false;
    }

    void OnClick()
    {
    }

    void OnUpdate(float ts)
    {
        if (Input.IsKeyPressed(KeyCode.C) && !keyPressed)
        {
            keyPressed = true;

            Scene.SetRenderColliders(!Scene.GetRenderColliders());
        }
        else if(!Input.IsKeyPressed(KeyCode.C))
            keyPressed = false;
    }
}