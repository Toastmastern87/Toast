﻿using System;
using System.Runtime.InteropServices;

namespace Toast
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X;
        public float Y;
        public float Z;

        public Vector3(float scalar) 
        {
            X = Y = Z = scalar;
        }

        public Vector3(float x, float y, float z) 
        {
            X = x;
            Y = y;
            Z = z;
        }

        public override string ToString()
        {
            return "Vector3[" + X + ", " + Y + ", " + Z + "]";
        }

        public static float Length(Vector3 vec) 
        {
            return (float)Math.Sqrt((vec.X * vec.X) + (vec.Y * vec.Y) + (vec.Z * vec.Z));
        }

        public static Vector3 Normalize(Vector3 vec)
        {
            return new Vector3(vec.X / Length(vec), vec.Y / Length(vec), vec.Z / Length(vec));
        }

        public static float Dot(Vector3 vecOne, Vector3 vecTwo)
        {
            return vecOne.X * vecTwo.X + vecOne.Y * vecTwo.Y + vecOne.Z * vecTwo.Z;
        }

        public static Vector3 Cross(Vector3 vecOne, Vector3 vecTwo)
        {
            return new Vector3(vecOne.Y * vecTwo.Z - vecOne.Z * vecTwo.Y, vecOne.Z * vecTwo.X - vecOne.X * vecTwo.Z, vecOne.X * vecTwo.Y - vecOne.Y * vecTwo.X);
        }

        public static float AngleNormalizedVectors(Vector3 vecOne, Vector3 vecTwo)
        {
            double dot = Dot(vecOne, vecTwo);
            double lengthVecOne = Length(vecOne);
            double lengthVecTwo = Length(vecTwo);

            if (dot > 1)
                dot = 1.0f;

            return (float)Math.Acos(dot); /// (lengthVecOne * lengthVecTwo)
        }

        public static Vector3 Rotate(Vector3 point, Vector3 axis, float angle)
        {
            Vector3 v1 = axis * (Vector3.Dot(point, axis) / Vector3.Dot(axis, axis));
            Vector3 v2 = point - v1;

            Vector3 w = Vector3.Cross(axis, v2);

            float x1 = (float)Math.Cos(angle) / Vector3.Length(v2);
            float x2 = (float)Math.Sin(angle) / Vector3.Length(w);

            Vector3 v3 = (v2 * x1 + w * x2) * Vector3.Length(v2);

            Vector3 ret = v3 + v1;

            return ret;
        }

        public static Vector3 operator +(Vector3 vectorOne, Vector3 vectorTwo) 
        {
            return new Vector3(vectorOne.X + vectorTwo.X, vectorOne.Y + vectorTwo.Y, vectorOne.Z + vectorTwo.Z);
        }

        public static Vector3 operator +(Vector3 vectorOne, float value)
        {
            return new Vector3(vectorOne.X + value, vectorOne.Y + value, vectorOne.Z + value);
        }

        public static Vector3 operator -(Vector3 vectorOne, float value)
        {
            return new Vector3(vectorOne.X - value, vectorOne.Y - value, vectorOne.Z - value);
        }

        public static Vector3 operator -(Vector3 vectorOne, Vector3 vectorTwo)
        {
            return new Vector3(vectorOne.X - vectorTwo.X, vectorOne.Y - vectorTwo.Y, vectorOne.Z - vectorTwo.Z);
        }

        public static Vector3 operator *(Vector3 vectorOne, float value)
        {
            return new Vector3(vectorOne.X * value, vectorOne.Y * value, vectorOne.Z * value);
        }
    }
}