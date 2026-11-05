window.BENCHMARK_DATA = {
  "lastUpdate": 1793893662060,
  "repoUrl": "https://github.com/QuentinStoll/SpectraDsp",
  "entries": {
    "Profiler audio": [
      {
        "commit": {
          "author": {
            "email": "114921049+QuentinStoll@users.noreply.github.com",
            "name": "Quentin Stoll",
            "username": "QuentinStoll"
          },
          "committer": {
            "email": "114921049+QuentinStoll@users.noreply.github.com",
            "name": "Quentin Stoll",
            "username": "QuentinStoll"
          },
          "distinct": true,
          "id": "3a66b5065dc9ef20a46e7c67c340ca90c7e5639a",
          "message": "test: gate 48 kHz audio contracts and publish latency history\n\nEOF",
          "timestamp": "2026-11-05T16:33:56+01:00",
          "tree_id": "1268fda0daf3ea393ac9d7b2d8c1b92c9662451a",
          "url": "https://github.com/QuentinStoll/SpectraDsp/commit/3a66b5065dc9ef20a46e7c67c340ca90c7e5639a"
        },
        "date": 1793893661613,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "Pedal latency",
            "value": 3,
            "unit": "samples",
            "extra": "48 kHz"
          },
          {
            "name": "Harmonizer latency",
            "value": 1056,
            "unit": "samples",
            "extra": "48 kHz"
          },
          {
            "name": "Drop latency",
            "value": 960,
            "unit": "samples",
            "extra": "48 kHz"
          },
          {
            "name": "Gate lookahead",
            "value": 384,
            "unit": "samples",
            "extra": "48 kHz"
          },
          {
            "name": "Pedal realtime ratio",
            "value": 0.0060756,
            "unit": "x realtime",
            "extra": "48 kHz"
          },
          {
            "name": "Harmonizer realtime ratio",
            "value": 0.2977143,
            "unit": "x realtime",
            "extra": "48 kHz"
          },
          {
            "name": "Drop realtime ratio",
            "value": 0.00624065,
            "unit": "x realtime",
            "extra": "48 kHz"
          }
        ]
      }
    ]
  }
}