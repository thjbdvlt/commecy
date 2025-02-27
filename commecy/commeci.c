/* commecy -- normalizer for french for spacy.
 * Copyright (C) 2024,2025  thjbdvlt
 *
 * commecy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * commecy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with commecy.  If not, see <https://www.gnu.org/licenses/>.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <regex.h>

/* structs for suffixes */
typedef struct Suffixes
{
  Py_UCS4* str;
  int len;
  int n_opts;
  const struct Suffixes* const* opts;
} suffix;

/* "-rice-s", "-rice-x", "-rice-x-s" */
const suffix suff_plural_s = { U"s", 1, 0, NULL };
const suffix* const suff_plural[] = {
  &suff_plural_s,
};
const suffix suff_nonbinary_s = { U"x", 1, 1, suff_plural };
const suffix* const suff_nonbinary[] = {
  &suff_nonbinary_s,
};
const suffix* const suff_plural_nonbinary[] = {
  &suff_plural_s,
  &suff_nonbinary_s,
};

/* feminine suffixes */
const suffix suff_feminine[] = {
#define N_SUFF_FEMININE 17
#define xs 2, suff_plural_nonbinary
  { U"e", 1, xs },
  { U"te", 2, xs },
  { U"euse", 4, xs },
  { U"ese", 3, xs },
  { U"ère", 3, xs },
  { U"Ère", 3, xs },
  { U"ice", 3, xs },
  { U"rice", 4, xs },
  { U"trice", 5, xs },
  { U"ale", 3, xs },
  { U"ne", 2, xs },
  { U"ive", 3, xs },
  { U"ve", 2, xs },
  { U"esse", 4, xs },
  { U"oresse", 6, xs },
  { U"se", 2, xs },
  { U"fe", 2, xs },
// { U"le", 2, xs }, // would requires condition BEFORE
#undef xs
};

int
iswordch(Py_UCS4 c)
{
  return (Py_UNICODE_ISALPHA(c) || c == L'·');
}

Py_UCS4*
match_suff(Py_UCS4* str, const suffix* suffix, int max, Py_UCS4 sep)
{

  int i;
  Py_UCS4* endptr;
  Py_UCS4 c;
  Py_UCS4* x;

  /* if there is not enough space for the suffix, it does not match.
   */
  if (max < suffix->len)
    return NULL;

  i = suffix->len;

  /* ensure all the characters from the suffixes are in the string.
   */
  for (i = 0; i < suffix->len; i++) {
    if (str[i] != suffix->str[i])
      return NULL;
  }

  /* pointer to the end of the string. */
  endptr = &str[i];

  /* if the string reached the end, it matches. */
  if ((max - i) == 0)
    return endptr;

  /* else, get the character class of the next characters. */
  c = str[i];

  /* if the next character is not a a word character, then it's a
   * match. and if it's not the separator, the match don't go
   * forward, so the function can end. */
  if (!iswordch(c) && c != sep)
    return endptr;

  /* if the next char is the suffix separator, then the match could
   * continue. if it doesn't, it's still a match. */
  else if (c == sep)
    i++;

  /* if it's a word character, the match could continue, too. but if
   * it does not, it's not a match at all. */
  else
    endptr = NULL;

  /* try to match every optional suffix. */
  for (int iopt = 0; iopt < suffix->n_opts; iopt++) {

    /* recursive call, like in "auteur-rice-x-s". */
    x = match_suff(&str[i], suffix->opts[iopt], (max - i), sep);
    if (x)
      return x;
  }

  return endptr;
}

int
is_incl_suff(Py_UCS4* s, int len, Py_UCS4 sep)
{
  Py_UCS4* x = NULL;
  for (int i = 0; i < N_SUFF_FEMININE; i++) {
    x = match_suff(s + 1, &suff_feminine[i], len, sep);
    if (x)
      return (int)(x - s);
  }
  return 0;
}

/* qqquuuooooiiiiiiiii??? */
int
reduce_3_repeated_letters(Py_UCS4* s, int len)
{
  Py_UCS4 c;
  int y;
  int x;
  int i;

  for (i = len; i > 1; i--) {
    c = s[i];

    if (c == s[i - 1] && c == s[i - 2]) {
      x = i;

      while (i > 0) {
        if (s[i - 1] != c)
          break;
        i--;
      }

      for (y = i; x <= len;) {
        s[y] = s[x];
        x++;
        y++;
      }

      len = y - 1; // why -1?
    }
  }
  return len;
}

/* this function does NOT replace reduce_2_repeated_letters, and is
 * not replaced by it. it serves a different purpose. */
int
reduce_2_repeated_letters(Py_UCS4* s, int len)
{
  Py_UCS4 c;
  int y;
  int x;
  int i;

  for (i = len; i >= 1; i--) {
    c = s[i];

    if (c == s[i - 1]) {
      x = i;

      while (i > 0) {
        if (s[i - 1] != c)
          break;
        i--;
      }

      for (y = i; x <= len;) {
        s[y] = s[x];
        x++;
        y++;
      }

      len = y - 1;
    }
  }
  return len;
}

int
replace_chars(Py_UCS4* s, Py_UCS4* new, int len)
{
  Py_UCS4 c;
  int i, y;
  int sufflen;
  int tmplen;

  for (i = 0, y = 0; i < len; i++) {

    c = s[i];
    switch (c) {

      // (re)présenter
      case L'(':
      case L')':
      case L'[':
      case L']':
      case L'}':
      case L'{':
        // do not remove parenthese when it's the only character
        if (len == 1)
          new[y++] = c;
        break;

      // jusqu'ici
      case L'‘':
      case L'’':
      case L'`':
        new[y++] = L'\'';
        break;

      // double quotes
      case L'«':
      case L'»':
      case L'“':
      case L'”':
        new[y++] = L'"';
        break;

      // hyphens (tirets longs et moyens)
      case L'—':
      case L'–':
        new[y++] = L'-';
        break;

      // auteur·rices
      case L'-':
      case L'.':
      case L'·':
        sufflen = is_incl_suff(&s[i], len - i, c);
        if (sufflen) {
          new[y++] = L'·';
          for (int x = 0; x < sufflen; x++) {
            switch (s[i]) {
              case L'·':
              case L'-':
              case L'.':
              case L'x':
                i++;
                ;
                break;
              default:
                new[y++] = s[i++];
                break;
            }
          }
          i--;
        } else {
          new[y++] = c;
        }
        break;

      // ligatures
      case L'œ':
      case L'æ':
        new[y++] = (c == L'œ') ? L'o' : L'a';
        new[y++] = L'e';
        break;

      default:
        new[y++] = c;
        break;
    }
  }
  return y;
}

static PyObject*
normalize(PyObject* self, PyObject* arg)
{
  /* input value and output values */
  PyObject *input, *ret;
  Py_ssize_t len, _len;
  Py_UCS4 *s, *new;

  /* get the parameter value */
  if (!PyArg_Parse(arg, "U:normalize", &input)) {
    PyErr_BadArgument();
    return NULL;
  }

  /* get the length of the string */
  len = PyUnicode_GetLength(input);
  if ((len = PyUnicode_GetLength(input)) == -1) {
    PyErr_BadArgument();
    return NULL;
  }

  s = PyUnicode_AsUCS4Copy(input);
  if (!s)
    return PyErr_NoMemory();

  new = (Py_UCS4*)malloc(sizeof(Py_UCS4*) * ((size_t)len + 2) * 2);
  len = replace_chars(s, new, (int)len);
  len = reduce_3_repeated_letters(new, (int)len);

  /* create a new string and populate it with characters */
  ret = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, new, len);

  PyMem_FREE(s);
  PyMem_FREE(new);

  return ret;
}

static PyObject*
dediacritic_dedoubler(PyObject* self, PyObject* arg)
{

  /* input value and output values */
  PyObject *input, *ret;
  Py_ssize_t len, _len;
  Py_UCS4 *s, *new;

  /* get the parameter value */
  if (!PyArg_Parse(arg, "U:dediacritic", &input)) {
    PyErr_BadArgument();
    return NULL;
  }

  /* get the length of the string */
  len = PyUnicode_GetLength(input);
  if ((len = PyUnicode_GetLength(input)) == -1) {
    PyErr_BadArgument();
    return NULL;
  }

  s = PyUnicode_AsUCS4Copy(input);
  if (!s)
    return PyErr_NoMemory();

  new = (Py_UCS4*)malloc(sizeof(Py_UCS4*) * ((size_t)len + 2) * 2);

  Py_UCS4 c;
  for (int i=0; i<=len; i++) {
    c = s[i];
    switch (c) {
      case L'é':
      case L'è':
      case L'ê':
      case L'ë':
        c = 'e';
        break;
      case L'â':
      case L'à':
      case L'ä':
        c = 'a';
        break;
      case L'î':
      case L'ï':
        c = 'i';
        break;
      case L'ô':
      case L'ö':
        c = 'o';
        break;
      case L'ù':
      case L'û':
      case L'ü':
        c = 'u';
        break;
      default:
        break;
    }
    new[i] = c;
  }

  len = reduce_2_repeated_letters(new, len);
  ret = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, new, len);
  // free(new);
  PyMem_FREE(new);

  return ret;
}

/* informations about the module, so it can be called from within
 * python. */
static PyMethodDef commeci_methods[] = {
  { "normalize",
    normalize,
    METH_O,
    "Normalize a lowercased token." },
  { "dediacritic",
    dediacritic_dedoubler,
    METH_O,
    "Remove french diacritics on letters a,e,i,o,u and repeated letters." },
  { NULL, NULL, 0, NULL }
};

static struct PyModuleDef commeci_module = {
  PyModuleDef_HEAD_INIT,
  "commeci",
  "",
  -1,
  commeci_methods,
  NULL,
  NULL,
  NULL,
  NULL,
};

PyMODINIT_FUNC
PyInit_commeci(void)
{
  return PyModule_Create(&commeci_module);
}
