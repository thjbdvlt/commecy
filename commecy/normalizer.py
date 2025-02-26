# commecy -- normalizer for french for spacy.
# Copyright (C) 2024,2025  thjbdvlt
#
# commecy is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# commecy is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with commecy.  If not, see <https://www.gnu.org/licenses/>.


import spacy
from spacy.lookups import Table
import pkgutil
from .commeci import normalize, dediacritic


def _getdata(filename: str) -> list[str]:
    """Get data from file.

    Args:
        filename (str)

    Returns (list[str]): lines.
    """

    return (
        pkgutil.get_data(__name__, f"data/{filename}")
        .decode()
        .strip()
        .split("\n")
    )


class CommeCyNormalizer:
    def __init__(self, nlp, name) -> None:
        """Initialize a Normalizer.

        Args:
            nlp (Language)

        Returns (None)
        """

        self.name = name
        self.table = Table()
        self.table_dedia = Table()

        self.table_dedia.set("meme", "même")
        self.table_dedia.set("memes", "mêmes")

        for x in _getdata("words.txt"):
            self.table.set(x, x)

            y = dediacritic(x)

            if x == y:
                self.table_dedia.set(y, x)

            elif y not in self.table_dedia:
                self.table_dedia.set(y, x)

    def add(self, norm: str, forms: list[str]) -> None:
        """Associate a norm with a list of forms.

        Args:
            norm (str)
            forms (list[str])

        Returns (None)
        """

        for i in forms:
            self.table.set(i, norm)

    def normalize(self, s: str) -> None:
        """Set the norm to a Token (i.e. to its Lexeme).

        Args:
            token (Token)

        Returns (None)
        """

        table = self.table

        # check if the form is registered as-is.
        if s in table:
            norm = table[s]
            return norm

        # lowercase the form and check again
        s_lower = s.lower()
        if s_lower in table:
            norm = table[s_lower]
            self.add(norm, [s])
            return norm

        # normalize the form (remove parenthese, repeated chars, ...)
        s_norm = normalize(s_lower)
        if s_norm in table:
            norm = table[s_norm]
            self.add(norm, [s, s_lower])
            return norm

        # process each part of compound words separately.
        # but do not remove the `-` in `-nous` (subject-verb inversion).
        if "-" in s_norm and s_norm[0] != "-":
            norm = self.normalize_compound(s_norm)
            self.add(norm, [s, s_lower, s_norm])
            return norm

        # remove diacritic to ensure it's not a disorthographic form
        s_dedi = dediacritic(s_norm)
        if s_dedi in self.table_dedia:
            # here is the issue, it seems.
            norm = self.table_dedia[s_dedi]
            self.add(norm, [s, s_lower, s_norm])
            return norm

        # add the normalized-by-rules form to the lookup table
        norm = s_norm
        self.add(norm, [s, s_lower, s_norm, s_dedi])
        return norm

    def normalize_compound(self, compound) -> str:
        """Normalize each component of a compound word.

        Args:
            compound (str): the pre-normalized compound word.

        Returns (str): the normalized compound word.
        """

        # split the compound into its parts
        parts = compound.split("-")

        # normalize each parts
        for n, i in enumerate(parts):
            if i in self.table:
                parts[n] = self.table[i]
                continue
            d = dediacritic(i)
            if d in self.table_dedia:
                parts[n] = self.table_dedia[d]

        # join everything
        return "-".join(parts)

    def __call__(self, doc) -> None:
        """Normalize a Doc.

        Args:
            doc (Doc)

        Returns (Doc)
        """

        for token in doc:
            token.norm_ = self.normalize(token.text)
        return doc

    def to_disk(self, path, exclude=tuple()):
        """Save the component data to disk."""

        path = spacy.util.ensure_path(path)

        if not path.exists():
            path.mkdir()
        for i in ("table", "table_dedia"):
            idx_path = path / i
            with idx_path.open("wb") as f:
                f.write(getattr(self, i).to_bytes())

    def from_disk(self, path, *, exclude=tuple()):
        """Load the component data from disk."""

        for i in ("table", "table_dedia"):
            idx_path = path / i
            table = spacy.lookups.Table()
            with idx_path.open("rb") as f:
                table.from_bytes(f.read())
            setattr(self, i, table)


@spacy.Language.factory("commecy_normalizer")
def create_commecy_normalizer(nlp, name):
    return CommeCyNormalizer(nlp, name)
