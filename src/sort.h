#pragma once
/*
MIT No Attribution

Copyright 2025 Tré Dudman

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

static void sort_int(int* a, int N)
{
    int i, j;
    int temp;
    for (i = 0; i < (N - 1); ++i)
    {
        for (j = 0; j < N - 1 - i; ++j)
        {
            int cond1 = a[j] > a[j + 1];
            if (cond1)
            {
                temp     = a[j + 1];
                a[j + 1] = a[j];
                a[j]     = temp;
            }
        }
    }
}

static void sort_char(char* a, int N)
{
    int  i, j;
    char temp;
    for (i = 0; i < (N - 1); ++i)
    {
        for (j = 0; j < N - 1 - i; ++j)
        {
            int cond1 = a[j] > a[j + 1];
            if (cond1)
            {
                temp     = a[j + 1];
                a[j + 1] = a[j];
                a[j]     = temp;
            }
        }
    }
}

// Note: Windows qsort_s is not the same as C11 qsort_s. F
//       MacOS qsort_r is not the same as linux qsort_r. F
//       Fortunately the compare function used by Windows qsort_s is identical to Apples qsort_r, the only difference is
//       the parameter order to qsort_x.
// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/qsort-s?view=msvc-170
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/qsort_r.3.html
// https://en.cppreference.com/w/c/algorithm/qsort
static inline void xqsort_s(
    void*              base,
    unsigned long long num_elements,
    unsigned long long size_elements,
    int (*compare_func)(void* ctx, void const* a, void const* b),
    void* ctx)
{
#ifdef _WIN32
    extern void qsort_s(
        void*              _Base,
        unsigned long long _NumOfElements,
        unsigned long long _SizeOfElements,
        int (*_CompareFunction)(void*, void const*, void const*),
        void* _Context);
    qsort_s(base, num_elements, size_elements, compare_func, ctx);
#elif defined(__APPLE__)
    extern void qsort_r(
        void*         _base,
        unsigned long _nel,
        unsigned long _width,
        void*         _thunk,
        int (*_compar)(void*, const void*, const void*));
    qsort_r(base, num_elements, size_elements, ctx, compare_func);
#else
#error "Platform unsupported"
#endif
}