# Clean-Room Rules

Outframe must stay independent from GPL codebases so it can keep the Apache-2.0
license.

## Allowed

- Reading public documentation for Windows APIs.
- Studying general product behavior at a high level.
- Using original code written for this repository.
- Depending on libraries with compatible licenses.
- Creating original shaders, assets, icons, and UI text.

## Not Allowed

- Copying code from Magpie or another GPL project.
- Translating GPL source file by file into new code.
- Copying HLSL shaders, icons, images, project files, or docs from Magpie.
- Reusing internal architecture because it was seen in GPL source.
- Importing GPL snippets from forums, gists, or examples without compatible
  licensing.

## Engineering Practice

- For every dependency, record the license.
- For every shader, record its origin.
- Keep implementation notes focused on requirements and behavior, not copied
  source structure.
- When in doubt, write a smaller original version first.
