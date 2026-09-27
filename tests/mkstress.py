"""Build tests/data/stress.pgn from parity.pgn, exercising the paths parity
lacks: duplicate games (same and different players), error terminations,
and FEN-start games (made by a stock pgn-extract's --dropply).

    python tests/mkstress.py tests/data/parity.pgn <stock pgn-extract.exe> tests/data/stress.pgn

Seeded, so the same parity.pgn and pgn-extract (v24-11 was used for the
goldens) reproduce the same file; tests/check.sh checks its hash."""
import random, re, subprocess, sys

src, pe, out = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(src, encoding='latin-1').read().replace('\r\n', '\n')
games = [g.strip() + '\n' for g in re.split(r'\n\n(?=\[Event )', text) if g.strip()]
rng = random.Random(7)
decisive = [g for g in games if '[Result "1-0"]' in g or '[Result "0-1"]' in g]

def set_tag(g, tag, val):
    if re.search(r'^\[%s "' % tag, g, re.M):
        return re.sub(r'^\[%s "[^"]*"\]' % tag, '[%s "%s"]' % (tag, val), g, flags=re.M)
    return g.replace('\n\n', '\n[%s "%s"]\n\n' % (tag, val), 1)

extra = []
# exact duplicates of decisive games (dedup must drop them)
extra += rng.sample(decisive, 80)
# same moves, different players (dup key ignores tags)
players = sorted(set(re.findall(r'^\[White "([^"]*)"', text, re.M)))
for g in rng.sample(decisive, 40):
    extra.append(set_tag(g, 'White', rng.choice(players)))
# error terminations, some of which also duplicate a clean game
for g in rng.sample(decisive, 60):
    extra.append(set_tag(g, 'Termination',
                         rng.choice(['time forfeit', 'abandoned', 'rules infraction',
                                     'illegal move', 'normal'])))
# FEN starts
fen_src = '\n\n'.join(rng.sample(decisive, 120)) + '\n'
open(out + '.fensrc', 'w', encoding='latin-1', newline='\n').write(fen_src)
for drop in (9, 16, 30):
    r = subprocess.run([pe, '--quiet', '--dropply', str(drop), out + '.fensrc'],
                       capture_output=True, text=True, encoding='latin-1')
    fg = [g.strip() + '\n' for g in re.split(r'\n\n(?=\[Event )', r.stdout) if g.strip()]
    extra += fg[:40]

allg = games + extra
rng.shuffle(allg)
open(out, 'w', encoding='latin-1', newline='\n').write('\n'.join(allg))
print(len(games), 'base +', len(extra), 'extra; FEN games:',
      sum('[FEN ' in g for g in allg))
