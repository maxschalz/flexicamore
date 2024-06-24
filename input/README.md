# Flexible Cycamore
## input
This directory contains an example input file of a *small* (nonsensical)
nuclear fuel cycle to showcase how the `flexicamore` archetypes can be used.
- `simple_enrichment.json`: Two `cycamore::Sink` agents providing feed material
  for one `flexicamore::FlexibleEnrichment` agent which produces enriched
  uranium.
- `source_enrichment_sink.json`: Two `cycamore::Source` producing LEU, one
  `flexicamore::FlexibleSource` producing natural uranium, and one
  `flexicamore::FlexibleSource` producing LEU.
  These materials are enriched by a `flexicamore::FlexibleEnrichment` and sent
  to a `flexicamore::FlexibleSink` and/or a `flexicamore::FlexibleStorage`.
  The `FlexibleStorage` in turn sends its material to yet another
  `FlexibleSink`.

  The output file, `cyclus_source_enrichment_sink.sqlite`, and the output printed to the commandline,
  `stdout_stderr.txt`, are provided for convenience and have been obtained using
  the above-mentioned input file.
- `simple_flexiblestorage.json`: Two `cycamore::Source` sending material to a
  `flexicamore::FlexibleStorage` which forwards the material to a
  `cycamore:::Sink`.
