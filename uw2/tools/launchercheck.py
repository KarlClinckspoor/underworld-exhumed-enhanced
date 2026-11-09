"""Checks the fix launcher-enchants (docs/ENHANCEMENTS.md; docs/FINDINGS.md, "A launcher's
enchantment does nothing"). player_fire (combat/MISSILE.C) reads the launcher in the weapon hand
only to choose its ammunition, and missile_newhit (motion/OBJPHYS.C) takes a missile's damage from
the ammunition's Missile[] entry and the Missile skill: a bow, crossbow or sling "of Damage" or
"of Accuracy" does nothing. With --enhance launcher-enchants the launcher's class 12 enchantment
is applied with DoPlayerWeapon's arithmetic (combat/COMBAT.C): effects 0 to 3 add 2 * effect + 1
to the missile's damage before the dice (the effects the game names Accuracy), 4 to 7 add
2 * effect - 7 to the Missile skill in the one skill check a missile makes (named Damage).

What it changes is damage, which the dice decide, so the port's missile probe
(UW2PORT_MISSILE_PROBE, port only) reports the inputs on stderr each time a missile the player
shot strikes: 'missile-probe: ammo A base B skill S launcher L effect E bonus-damage D bonus-hit H
check C grade G scale K damage N', the ammunition, its damage, the Missile skill, the item in the
weapon hand, the launcher's effect (-1 none), the two bonuses, the value the skill check is made
at, its result, the scale it gives and the damage before the dice.

The setup session tests/replay/setup/launcher-enchants (port-made, with a test's cheats) writes a
saved game: a new character facing a door, with a crossbow of effect 3 (named "of Unsurpassed
Accuracy", 7 damage), a bow of effect 7 (named "of Unsurpassed Damage", 7 to hit), bolts and
arrows. The session
tests/replay/enhanced/launcher-enchants loads it, shoots two bolts with the crossbow and two arrows
with the bow at the door. It is replayed with the probe as it was recorded (with the fix), and
again with its enhancements chunk naming skip-intro alone (the original).

    python3 tools/launchercheck.py

Passes when both replays end by themselves with the same four hits (two bolts from the crossbow,
then two arrows from the bow); with the fix each bolt's damage is 7 more and each arrow's check is
made at the skill plus 7, without it neither launcher adds anything; and in every hit the damage
is the ammunition's damage times the scale the check's result gives, over 256, plus the damage
bonus. Exit status 0 when every check passes."""
import os, re, sys, struct, shutil, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import replay as R

SESSIONS = os.path.join(os.path.dirname(here), 'tests', 'replay')
SETUP = os.path.join(SESSIONS, 'setup', 'launcher-enchants', 'session.rec')
SESSION = os.path.join(SESSIONS, 'enhanced', 'launcher-enchants', 'session.rec')
PROBE = 'UW2PORT_MISSILE_PROBE'
FIX = 'launcher-enchants'
SAVE = 'SAVE1'                          # slot 1
# the hits the session makes: (ammunition, the launcher in hand, its effect, damage bonus, to-hit bonus)
BOLT, ARROW, CROSSBOW, BOW = 1, 2, 0x1A, 0x19
HITS = [(BOLT, CROSSBOW, 3, 7, 0)] * 2 + [(ARROW, BOW, 7, 0, 7)] * 2
FIELDS = ['ammo', 'base', 'skill', 'launcher', 'effect', 'bonus-damage', 'bonus-hit', 'check', 'grade', 'scale', 'damage']


def names_of(data):
    """The names in a recording's enhancements chunk, or None if it has none. In formats 5 and 6
    the chunk follows the 12-byte header: stream 9, a 16-bit length, the names separated by
    commas (as replay.py's dos_can_replay reads it)."""
    if len(data) < 15 or data[4] not in (5, 6) or data[12] != 9: return None
    n = struct.unpack_from('<H', data, 13)[0]
    return data[15:15 + n].decode('ascii').split(',')


def without_fix(data):
    """The recording with FIX taken out of its enhancements chunk, or None if it has none."""
    names = names_of(data)
    if names is None or FIX not in names: return None
    n = struct.unpack_from('<H', data, 13)[0]
    kept = ','.join(x for x in names if x != FIX).encode()
    return data[:12] + b'\x09' + struct.pack('<H', len(kept)) + kept + data[15 + n:]


def probed(log):
    """The probe's lines as dicts of FIELDS."""
    pat = 'missile-probe: ' + ' '.join(f'{f} (-?\\d+)' for f in FIELDS)
    return [dict(zip(FIELDS, map(int, m.groups()))) for m in re.finditer(pat, log)]


def scale_of(h):
    """missile_newhit's scale for the skill and the check's result: Missile * 8 + 0xC0, less 0x80 on
    a critical failure (-1), plus 0xC0 on a critical success (2)."""
    return (h['skill'] << 3) + 0xC0 + {-1: -0x80, 2: 0xC0}.get(h['grade'], 0)


def run_port(rec, out, stage=None, probe=False):
    """R.run_port, with the probe on or off (UW2's run_port takes the port's environment from
    this process's)."""
    if probe: os.environ[PROBE] = '1'
    else: os.environ.pop(PROBE, None)
    try:
        return R.run_port(rec, out, [], stage=stage, quiet=True)
    finally:
        os.environ.pop(PROBE, None)


def main():
    bad, done = [], []
    def check(what, ok, detail=''):
        done.append(what)
        print(('ok    ' if ok else 'FAIL  ') + what + ('' if ok or not detail else f'\n      {detail}'))
        if not ok: bad.append(what)
    work = tempfile.mkdtemp(prefix='launchercheck-')
    try:
        setup = os.path.join(work, 'setup')
        rc = run_port(SETUP, setup)
        save = os.path.join(setup, SAVE)
        check(f'setup/{FIX} replayed, writing its saved game ({SAVE})', rc == 0 and os.path.isdir(save), f'status {rc}')
        if bad: return 1
        stage = os.path.join(work, 'stage')
        shutil.copytree(save, os.path.join(stage, SAVE))
        data = open(SESSION, 'rb').read()
        off = without_fix(data)
        check(f'the session carries {FIX}, and a copy without it can be made', off is not None and FIX not in names_of(off))
        if bad: return 1
        offrec = os.path.join(work, 'off.rec'); open(offrec, 'wb').write(off)
        shutil.copy(os.path.splitext(SESSION)[0] + '.cfg', os.path.splitext(offrec)[0] + '.cfg')  # its settings, which replay reads from beside it
        runs = {}
        for tag, rec in (('with the fix', SESSION), ('without it', offrec)):
            out = os.path.join(work, tag.replace(' ', '-'))
            rc = run_port(rec, out, stage, probe=True)
            log = open(os.path.join(out, 'port.log'), errors='replace').read()
            check(f'{tag}: replayed to its end (status 0)', rc == 0, f'status {rc}: ' + '\n      '.join(log.splitlines()[-6:]))
            hits = runs[tag] = probed(log)
            check(f'{tag}: two bolts from the crossbow, then two arrows from the bow, strike',
                  [(h['ammo'], h['launcher']) for h in hits] == [(a, l) for a, l, _, _, _ in HITS], hits)
            check(f'{tag}: each damage is the ammunition\'s times the check\'s scale, over 256, plus the bonus',
                  bool(hits) and all(h['scale'] == scale_of(h) and h['damage'] == (h['base'] * h['scale'] >> 8) + h['bonus-damage']
                                     for h in hits), hits)
        on, off = runs['with the fix'], runs['without it']
        if len(on) == len(HITS):
            check('with the fix: the crossbow adds 7 damage to each bolt, the bow 7 to the Missile skill\'s check for each arrow',
                  all((h['effect'], h['bonus-damage'], h['bonus-hit'], h['check']) == (e, d, b, h['skill'] + b)
                      for h, (_, _, e, d, b) in zip(on, HITS)), on)
        if len(off) == len(HITS):
            check('without it (the original): neither launcher adds anything',
                  all((h['effect'], h['bonus-damage'], h['bonus-hit'], h['check']) == (-1, 0, 0, h['skill']) for h in off), off)
        rc = run_port(SESSION, os.path.join(work, 'unprobed'), stage)
        log = open(os.path.join(work, 'unprobed', 'port.log'), errors='replace').read()
        check('the probe reports nothing when off', rc == 0 and 'missile-probe' not in log)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f'launchercheck: {len(done) - len(bad)} of {len(done)} checks pass')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
