## Day 01 (21-05-2026)

Learned about the replay tile majorly.
Every tile uses a Stem_run template to determine in what order the function gets called...
before_credit, after_credit, returnable_frag gets called in a loop

Each tile runs on a physical CPU core as configured in the related config.toml, so we can expect that every tile runs concurrently.

after_credit is basically the trigger. returnable_frag is the callback that gets called when an external tile finishes the work it was outsourced to do

ctx for a tile gets set upon boot up within privileged_init and unprivileged_init
