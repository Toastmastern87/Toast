using System.Runtime.InteropServices;

namespace Toast
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X;
        public float Y;

        public Vector2(float scalar)
        {
            X = Y = scalar;
        }

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        public static float Dot(Vector2 vecOne, Vector2 vecTwo)
        {
            return vecOne.X * vecTwo.X + vecOne.Y * vecTwo.Y;
        }

        public override string ToString()
        {
            return "Vector2[" + X + ", " + Y + "]";
        }

        public static Vector2 operator +(Vector2 vectorOne, Vector2 vectorTwo)
        {
            return new Vector2(vectorOne.X + vectorTwo.X, vectorOne.Y + vectorTwo.Y);
        }

        public static Vector2 operator +(Vector2 vectorOne, float value)
        {
            return new Vector2(vectorOne.X + value, vectorOne.Y + value);
        }
    }
}
