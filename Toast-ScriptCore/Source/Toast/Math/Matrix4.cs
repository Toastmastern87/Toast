using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Toast
{
    [StructLayout(LayoutKind.Explicit)]
    public struct Matrix4
    {
        [FieldOffset(0)] public float D00;
        [FieldOffset(4)] public float D10;
        [FieldOffset(8)] public float D20;
        [FieldOffset(12)] public float D30;
        [FieldOffset(16)] public float D01;
        [FieldOffset(20)] public float D11;
        [FieldOffset(24)] public float D21;
        [FieldOffset(28)] public float D31;
        [FieldOffset(32)] public float D02;
        [FieldOffset(36)] public float D12;
        [FieldOffset(40)] public float D22;
        [FieldOffset(44)] public float D32;
        [FieldOffset(48)] public float D03;
        [FieldOffset(52)] public float D13;
        [FieldOffset(56)] public float D23;
        [FieldOffset(60)] public float D33;

        public Matrix4(float value)
        {
            D00 = value; D10 = 0.0f; D20 = 0.0f; D30 = 0.0f;
            D01 = 0.0f; D11 = value; D21 = 0.0f; D31 = 0.0f;
            D02 = 0.0f; D12 = 0.0f; D22 = value; D32 = 0.0f;
            D03 = 0.0f; D13 = 0.0f; D23 = 0.0f; D33 = value;
        }

        public Matrix4(Matrix4 matrix)
        {
            D00 = matrix.D00; D10 = matrix.D10; D20 = matrix.D20; D30 = matrix.D30;
            D01 = matrix.D01; D11 = matrix.D11; D21 = matrix.D21; D31 = matrix.D31;
            D02 = matrix.D02; D12 = matrix.D12; D22 = matrix.D22; D32 = matrix.D32;
            D03 = matrix.D03; D13 = matrix.D13; D23 = matrix.D23; D33 = matrix.D33;
        }

        public Vector3 Translation
        {
            get { return new Vector3(D03, D13, D23); }
            set { D03 = value.X; D13 = value.Y; D23 = value.Z; }
        }

        public static Matrix4 Translate(Vector3 translation) 
        {
            Matrix4 result = new Matrix4(1.0f);
            result.D03 = translation.X;
            result.D13 = translation.Y;
            result.D23 = translation.Z;

            return result;
        }

        public static Matrix4 Scale(Vector3 scale)
        {
            Matrix4 result = new Matrix4(1.0f);
            result.D00 = scale.X;
            result.D11 = scale.Y;
            result.D22 = scale.Z;

            return result;
        }

        public static Matrix4 Scale(float scale)
        {
            Matrix4 result = new Matrix4(1.0f);
            result.D00 = scale;
            result.D11 = scale;
            result.D22 = scale;

            return result;
        }

        public static Matrix4 operator *(Matrix4 matrixOne, Matrix4 matrixTwo)
        {
            Matrix4 res;
            res.D00 = matrixOne.D00 * matrixTwo.D00 + matrixOne.D01 * matrixTwo.D10 + matrixOne.D02 * matrixTwo.D20 + matrixOne.D03 * matrixTwo.D30;
            res.D01 = matrixOne.D00 * matrixTwo.D01 + matrixOne.D01 * matrixTwo.D11 + matrixOne.D02 * matrixTwo.D21 + matrixOne.D03 * matrixTwo.D31;
            res.D02 = matrixOne.D00 * matrixTwo.D02 + matrixOne.D01 * matrixTwo.D12 + matrixOne.D02 * matrixTwo.D22 + matrixOne.D03 * matrixTwo.D32;
            res.D03 = matrixOne.D00 * matrixTwo.D03 + matrixOne.D01 * matrixTwo.D13 + matrixOne.D02 * matrixTwo.D23 + matrixOne.D03 * matrixTwo.D33;
                                                                                                                                             
            res.D10 = matrixOne.D10 * matrixTwo.D00 + matrixOne.D11 * matrixTwo.D10 + matrixOne.D12 * matrixTwo.D20 + matrixOne.D13 * matrixTwo.D30;
            res.D11 = matrixOne.D10 * matrixTwo.D01 + matrixOne.D11 * matrixTwo.D11 + matrixOne.D12 * matrixTwo.D21 + matrixOne.D13 * matrixTwo.D31;
            res.D12 = matrixOne.D10 * matrixTwo.D02 + matrixOne.D11 * matrixTwo.D12 + matrixOne.D12 * matrixTwo.D22 + matrixOne.D13 * matrixTwo.D32;
            res.D13 = matrixOne.D10 * matrixTwo.D03 + matrixOne.D11 * matrixTwo.D13 + matrixOne.D12 * matrixTwo.D23 + matrixOne.D13 * matrixTwo.D33;
                                                                                                                                             
            res.D20 = matrixOne.D20 * matrixTwo.D00 + matrixOne.D21 * matrixTwo.D10 + matrixOne.D22 * matrixTwo.D20 + matrixOne.D23 * matrixTwo.D30;
            res.D21 = matrixOne.D20 * matrixTwo.D01 + matrixOne.D21 * matrixTwo.D11 + matrixOne.D22 * matrixTwo.D21 + matrixOne.D23 * matrixTwo.D31;
            res.D22 = matrixOne.D20 * matrixTwo.D02 + matrixOne.D21 * matrixTwo.D12 + matrixOne.D22 * matrixTwo.D22 + matrixOne.D23 * matrixTwo.D32;
            res.D23 = matrixOne.D20 * matrixTwo.D03 + matrixOne.D21 * matrixTwo.D13 + matrixOne.D22 * matrixTwo.D23 + matrixOne.D23 * matrixTwo.D33;
                                                                                                                                            
            res.D30 = matrixOne.D30 * matrixTwo.D00 + matrixOne.D31 * matrixTwo.D10 + matrixOne.D32 * matrixTwo.D20 + matrixOne.D33 * matrixTwo.D30;
            res.D31 = matrixOne.D30 * matrixTwo.D01 + matrixOne.D31 * matrixTwo.D11 + matrixOne.D32 * matrixTwo.D21 + matrixOne.D33 * matrixTwo.D31;
            res.D32 = matrixOne.D30 * matrixTwo.D02 + matrixOne.D31 * matrixTwo.D12 + matrixOne.D32 * matrixTwo.D22 + matrixOne.D33 * matrixTwo.D32;
            res.D33 = matrixOne.D30 * matrixTwo.D03 + matrixOne.D31 * matrixTwo.D13 + matrixOne.D32 * matrixTwo.D23 + matrixOne.D33 * matrixTwo.D33;

            return res;
        }

        public static bool operator ==(Matrix4 matrixOne, Matrix4 matrixTwo)
        {
            bool res = true;

            res &= matrixOne.D00 == matrixTwo.D00;
            res &= matrixOne.D01 == matrixTwo.D01;
            res &= matrixOne.D02 == matrixTwo.D02;
            res &= matrixOne.D03 == matrixTwo.D03;

            res &= matrixOne.D10 == matrixTwo.D10;
            res &= matrixOne.D11 == matrixTwo.D11;
            res &= matrixOne.D12 == matrixTwo.D12;
            res &= matrixOne.D13 == matrixTwo.D13;

            res &= matrixOne.D20 == matrixTwo.D20;
            res &= matrixOne.D21 == matrixTwo.D21;
            res &= matrixOne.D22 == matrixTwo.D22;
            res &= matrixOne.D23 == matrixTwo.D23;

            res &= matrixOne.D30 == matrixTwo.D30;
            res &= matrixOne.D31 == matrixTwo.D31;
            res &= matrixOne.D32 == matrixTwo.D32;
            res &= matrixOne.D33 == matrixTwo.D33;

            return res;
        }

        public static bool operator !=(Matrix4 matrixOne, Matrix4 matrixTwo)
        {
            return !(matrixOne == matrixTwo);
        }

        public override string ToString()
        {
            return "Matrix4[" + D00 + ", " + D10 + ", " + D20 + ", " + D30 + "]\n" +
                   "       [" + D01 + ", " + D11 + ", " + D21 + ", " + D31 + "]\n" +
                   "       [" + D02 + ", " + D12 + ", " + D22 + ", " + D32 + "]\n" +
                   "       [" + D03 + ", " + D13 + ", " + D23 + ", " + D33 + "]";
        }

    }
}
