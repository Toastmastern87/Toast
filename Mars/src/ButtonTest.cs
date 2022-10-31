using System;

using Toast;

class ButtonTest : Entity
{
    private bool keyPressed;

    void OnCreate()
    {
    }

    void OnUpdate(float ts)
    {
    }

    void OnClick() 
    {
        Toast.Console.LogWarning("Button Click in C#!");
    }
}