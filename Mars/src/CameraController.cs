﻿using System;

using Toast;

class CameraController : Entity
{
    public float MinAltitude;
    public float MaxAltitude;

    private TransformComponent mTransform;
    private Vector3 mForward;
    private Vector3 mViewDir;
    private Vector3 mRightMovement;
    private Vector3 mRightViewDir;
    private Vector3 mUpMovement;
    private Vector3 mUpViewDir;

    private Vector2 mCursorPos;

    void OnCreate()
    {
        mTransform = GetComponent<TransformComponent>();

        mForward = new Vector3(0.0f, 1.0f, 0.0f);
        mUpMovement = new Vector3(0.0f, 0.0f, -1.0f);
        mRightMovement = new Vector3(1.0f, 0.0f, 0.0f);

        mViewDir = new Vector3(0.0f, 1.0f, 0.0f);
        mUpViewDir = new Vector3(0.0f, 0.0f, -1.0f );
        mRightViewDir = new Vector3(1.0f, 0.0f, 0.0f);
    }

    void OnUpdate(float ts)
    {
        Vector3 translation = new Vector3(mTransform.Transform.D03, mTransform.Transform.D13, mTransform.Transform.D23);

        mUpMovement = Vector3.Normalize(translation);

        float altitude = Vector3.Length(translation);
        float speed = (float)(12.7574f * Math.Exp(0.0005f * altitude));

        Vector2 newCursorPos = Input.GetMousePosition();

        if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
        {
            if (mCursorPos.X != newCursorPos.X)
            {
                mViewDir = Vector3.Rotate(mViewDir, mUpMovement, (newCursorPos.X - mCursorPos.X) * 0.003f);
                mUpViewDir = Vector3.Rotate(mUpViewDir, mUpMovement, (newCursorPos.X - mCursorPos.X) * 0.003f);
                mForward = Vector3.Rotate(mForward, mUpMovement, (newCursorPos.X - mCursorPos.X) * 0.003f);

                mRightMovement = Vector3.Cross(mUpMovement, mForward);
                mRightViewDir = Vector3.Cross(mUpViewDir, mViewDir);
            }
            if (mCursorPos.Y != newCursorPos.Y) 
            {
                mViewDir = Vector3.Rotate(mViewDir, mRightViewDir, (newCursorPos.Y - mCursorPos.Y) * 0.003f);
                mUpViewDir = Vector3.Cross(mViewDir, mRightViewDir);
            }
        }

        mCursorPos = newCursorPos;

        if (Input.GetMouseWheelDelta() != 0.0f)
        {
            float diffAltitude = 0.0f;
            float newAltitude = altitude + speed * ts * -Input.GetMouseWheelDelta();

            if (newAltitude > 9500.0f)
                diffAltitude = newAltitude - 9500.0f;

            if (newAltitude < 3400.0f)
                diffAltitude = newAltitude - 3400.0f;

            altitude = newAltitude - diffAltitude;
            translation = Vector3.Normalize(translation) * altitude;
        }

        if (Input.IsKeyPressed(KeyCode.W))
        {
            Vector3 startPos = Vector3.Normalize(translation);

            translation += Vector3.Normalize(mForward) * ts * speed;
            translation = Vector3.Normalize(translation) * altitude;

            Vector3 endPos = Vector3.Normalize(translation);

            mViewDir = Vector3.Rotate(mViewDir, mRightMovement, Vector3.AngleNormalizedVectors(startPos, endPos));
            mForward = Vector3.Rotate(mForward, mRightMovement, Vector3.AngleNormalizedVectors(startPos, endPos));
            mUpViewDir = Vector3.Rotate(mUpViewDir, mRightMovement, Vector3.AngleNormalizedVectors(startPos, endPos));
        }

        if (Input.IsKeyPressed(KeyCode.S))
        {
            Vector3 startPos = Vector3.Normalize(translation);

            translation -= Vector3.Normalize(mForward) * ts * speed;
            translation = Vector3.Normalize(translation) * altitude;

            Vector3 endPos = Vector3.Normalize(translation);

            mViewDir = Vector3.Rotate(mViewDir, mRightMovement, -Vector3.AngleNormalizedVectors(startPos, endPos));
            mForward = Vector3.Rotate(mForward, mRightMovement, -Vector3.AngleNormalizedVectors(startPos, endPos));
            mUpViewDir = Vector3.Rotate(mUpViewDir, mRightMovement, -Vector3.AngleNormalizedVectors(startPos, endPos));
        }

        if (Input.IsKeyPressed(KeyCode.A))
        {
            Vector3 startPos = Vector3.Normalize(translation);

            translation -= Vector3.Normalize(mRightMovement) * ts * speed;
            translation = Vector3.Normalize(translation) * altitude;

            Vector3 endPos = Vector3.Normalize(translation);

            mRightViewDir = Vector3.Rotate(mRightViewDir, mViewDir, Vector3.AngleNormalizedVectors(startPos, endPos));
            mRightMovement = Vector3.Rotate(mRightMovement, mForward, Vector3.AngleNormalizedVectors(startPos, endPos));
            mUpViewDir = Vector3.Rotate(mUpViewDir, mViewDir, Vector3.AngleNormalizedVectors(startPos, endPos));
        }

        if (Input.IsKeyPressed(KeyCode.D))
        {
            Vector3 startPos = Vector3.Normalize(translation);

            translation += Vector3.Normalize(mRightMovement) * ts * speed;
            translation = Vector3.Normalize(translation) * altitude;

            Vector3 endPos = Vector3.Normalize(translation);

            mRightViewDir = Vector3.Rotate(mRightViewDir, mViewDir, -Vector3.AngleNormalizedVectors(startPos, endPos));
            mRightMovement = Vector3.Rotate(mRightMovement, mForward, -Vector3.AngleNormalizedVectors(startPos, endPos));
            mUpViewDir = Vector3.Rotate(mUpViewDir, mViewDir, -Vector3.AngleNormalizedVectors(startPos, endPos));
        }

        mRightMovement = Vector3.Cross(mUpMovement, mForward);

        Matrix4 translationMatrix = Matrix4.Translate(translation);
        Matrix4 rotationMatrix = new Matrix4(1.0f);

        rotationMatrix.D00 = mRightViewDir.X;
        rotationMatrix.D10 = mRightViewDir.Y;
        rotationMatrix.D20 = mRightViewDir.Z;
        rotationMatrix.D02 = mViewDir.X;
        rotationMatrix.D12 = mViewDir.Y;
        rotationMatrix.D22 = mViewDir.Z;
        rotationMatrix.D01 = mUpViewDir.X;
        rotationMatrix.D11 = mUpViewDir.Y;
        rotationMatrix.D21 = mUpViewDir.Z;

        mTransform.Transform = translationMatrix * rotationMatrix;

        Input.SetMouseWheelDelta(0.0f);
    }
}