using System;

using Toast;

class ButtonTest : Entity
{
    private UIButtonComponent mButton;

    private Vector4 mColor;

    void OnCreate()
    {
    }

    void OnUpdate(float ts)
    {
    }

    void OnClick() 
    {
        mButton = GetComponent<UIButtonComponent>();

        mColor = mButton.Color;

        if (mColor == new Vector4(1.0f, 1.0f, 1.0f, 1.0f))
            mButton.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        else if (mColor == new Vector4(0.0f, 0.0f, 0.0f, 1.0f))
            mButton.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}