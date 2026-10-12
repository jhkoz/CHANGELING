# Frock coat reference

21 images of 6 garments from The Metropolitan Museum of Art, all flagged
`isPublicDomain` by the Met's collection API and downloaded under their Open
Access programme. A seventh candidate (object 92004, a 1906 frock coat) was
**not** open access and was deliberately skipped.

| Met object | Date | Views |
|---|---|---|
| 84364 | ca. 1840 | 6 |
| 91375 | 1830–49 | 3 |
| 91877 | ca. 1790 | 2 |
| 91908 | 1840s | 6 |
| 92005 | ca. 1881 (wedding) | 2 |
| 158133 | 1840–49 | 2 |

`manifest.json` carries each object's title, date, medium, credit line and
source URL. Every plane in the .blend also carries those as custom properties,
so an image can be traced back to its object without leaving Blender.

Rebuild the contact sheet with:

    blender -b --python Tools/make_reference_blend.py
