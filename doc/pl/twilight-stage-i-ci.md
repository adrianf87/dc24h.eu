# dc24h.eu-twilight — Etap I: tożsamość repozytorium i dowody CI

Ten dokument opisuje wyłącznie kwalifikację GitHub Actions. **Nie jest zgodą
na merge, release RC ani wdrożenie produkcyjne.** Podstawą jest Issue #1.

## Dwie oddzielne tożsamości

| Rola | Repozytorium | Przeznaczenie |
| --- | --- | --- |
| Źródło historyczne RC1 | `adrianf87/dc24h.eu-high-starter` | PR #243, Fix Pass, `v0.9.20-rc.1` i oryginalne dowody |
| Nowy projekt | `adrianf87/dc24h.eu-twilight` | Osobne PR, workflow runs, artefakty i bieżące dowody Twilight |

Nie wolno zastępować historycznych URL-i źródła adresami Twilight. Narzędzia
`scripts/beta08_github_evidence.py` i `scripts/beta09_artifact_evidence.py`
domyślnie używają źródłowego repozytorium dla zgodności z dawną weryfikacją.
Twilight wymaga **jawnego** `--repository` i trybu `twilight-pr` (albo `main`
po odrębnym merge, wtedy sprawdzane jest zdarzenie `push`).

## Przykład weryfikacji PR Etapu I

Wykonaj po ukończeniu **wszystkich pięciu workflow** przypisanych do dokładnego
SHA gałęzi PR, w lokalnym checkout odpowiadającym temu SHA:

```bash
python3 scripts/beta09_artifact_evidence.py \
  --root . \
  --sha "$(git rev-parse HEAD)" \
  --repository adrianf87/dc24h.eu-twilight \
  --mode twilight-pr \
  --branch fix/twilight-ci-repository-identity \
  --pr-number NUMER_PR \
  --output /tmp/twilight-stage-i-evidence.json
```

`NUMER_PR` zastąp numerem **otwartego PR w Twilight** do `main`.
Proces potrzebuje tokenu w zmiennej środowiskowej `GITHUB_TOKEN` z dostępem
tylko do odczytu metadanych GitHub Actions i artefaktów. Nie umieszczaj tokenu
w argumentach ani logach. Plik wyjściowy nie może już istnieć.

Weryfikator:

- sprawdza, czy wskazany PR należy do Twilight, kieruje do `main` i ma dokładny
  `head.ref` oraz `head.sha`;
- dwukrotnie odczytuje `HEAD` wskazanej gałęzi, odrzucając jej przesunięcie;
- wymaga kompletnego, zakończonego sukcesem zestawu zadań C++/WebAdmin,
  CodeQL, VM/systemd, TSAN/fuzz i storage, na właściwych hostowanych Ubuntu;
- nie uznaje `skipped`, obcego repozytorium, innego SHA, starej próby ani
  niepełnej listy zadań i artefaktów;
- pobiera ZIP-y artefaktów z GitHub, sprawdza ich SHA-256 i analizuje wyłącznie
  dozwolone, zanonimizowane raporty.

Testy negatywne: `tests/beta08_github_evidence_tests.py`,
`tests/beta09_artifact_evidence_tests.py` oraz
`tests/beta09_release_candidate_gate_tests.py`. Liczba zabezpieczonych
warunków `github.repository` wynosi **8** w **5** plikach workflow; pilnuje
ich `tests/beta_baseline_tests.py`.

## Ochrona `main` i odbiór

W chwili audytu Etapu I gałąź `main` w nowym repozytorium **nie była chroniona**.
Po uzyskaniu prawdziwych zielonych checków PR OWNER powinien ustawić w GitHub
Settings → Rules → Rulesets (lub Branch protection) dla `main` obowiązek PR,
review i konkretnych stabilnych status checks. Nie konfiguruj wymaganych
checków na podstawie wyników pominiętych ani historycznego RC1.

Wynik skryptu oraz zielone GitHub CI są dowodami technicznymi, **nie stanowią**
zgody OWNER na zmianę ochrony, merge lub wydanie nowego RC.
