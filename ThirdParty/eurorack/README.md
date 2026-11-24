# Sound engines by Emilie Gillet (MIT License)

BizGroove's Macro Synth plays sound engines written by Emilie Gillet, taken
from her open-source repositories:

- https://github.com/pichenettes/eurorack (folder `plaits/dsp`, commit f4cfa76)
- https://github.com/pichenettes/stmlib (commit d18def8)

Only the sound code is here, with each file's own MIT notice; see LICENSE.
BizGroove's changes:

- `plaits/user_data.h` is replaced: a computer has no module memory, so there
  is no user data.
- `stmlib/stmlib.h`: `__attribute__` does nothing on Microsoft's compiler.
- `plaits/resources.cc`: the empty (and unused) `lookup_table_i32_table`
  holds one null pointer, as Microsoft's compiler refuses empty arrays.
- Everything is compiled with `TEST` defined, as for the authors' own
  computer test program, so the module's processor instructions are not used.

These files are used here under the MIT License. BizGroove is not affiliated
with or endorsed by their author.
