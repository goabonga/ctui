# Textarea with external storage

`ctui_textarea_init()` still creates an owned, contiguous text buffer. Use
`ctui_textarea_init_source()` when the text lives in a piece table, a rope,
a mapped file or a cache of file blocks. The textarea keeps the descriptor
and editing state; it does not copy the document or construct a line index.

## Connecting a document

Implement these callbacks for your document:

| Callback | Contract |
| --- | --- |
| `length(ctx)` | Current number of bytes, at most `SIZE_MAX - 1`. |
| `lines(ctx)` | Current number of lines, at least one. A trailing newline starts an empty last line. |
| `line_start(ctx, line)` | Absolute byte offset of an existing line. Line zero starts at zero; later starts follow newlines and strictly increase. |
| `read(ctx, offset, out, n)` | Copy exactly `n` bytes, crossing your storage fragments as needed. Return false on failure. Each request is at most `CTUI_TEXTAREA_CHUNK` (4096) bytes. |
| `replace(ctx, pos, del, ins, n)` | Atomically replace `del` bytes with `n` bytes. Return false without changing the document on failure. Update length and line metadata before returning true. Optional: NULL makes the area read-only. |

For example, after implementing the `document_*` callbacks for your storage:

```c
ctui_textarea_source source = {
    .ctx = &document,
    .length = document_length,
    .lines = document_lines,
    .line_start = document_line_start,
    .read = document_read,
    .replace = document_replace, /* NULL for a read-only document */
};
ctui_textarea area;
if (!ctui_textarea_init_source(&area, &source)) {
    /* Missing callbacks or invalid size/endpoint metadata. */
}
/* Use the usual draw, key, mouse, selection, edit and undo functions. */
ctui_textarea_free(&area);
```

The descriptor is copied. The document stays caller-owned and must outlive
the textarea. Freeing the textarea never frees or closes the source. The
callbacks must not re-enter the same textarea. Changes must go through its
editing operations while attached; changing the source independently would
invalidate cursor offsets and undo records. To reload a different version,
free and initialise the area again.

The provider supplies accurate line metadata. It may maintain a sparse
index or an indexed tree; the textarea does not require an array containing
every line start. Building that metadata can involve scanning a file, but
need not retain its complete contents. File access, caching, saving and
handling external file changes belong to the provider/application.

## Reading text without flattening it

The existing pointer getters cannot expose fragmented storage:

- `ctui_textarea_text()` returns NULL for an external source.
- `ctui_textarea_line()` returns NULL and still reports the line length.
- Public `text`, `cap`, `starts` and `starts_cap` remain empty in source mode.
  Use functions rather than accessing those fields.

Use `ctui_textarea_line_bounds()` to get the start and byte length of a line,
and `ctui_textarea_read()` to copy a requested byte range into caller storage.
These functions also work with the original owned buffer. Read does not add
a NUL terminator and splits large requests into bounded source reads. False
means an invalid range or a source failure; the destination may then contain
a partial result. A zero-byte read at the end of the document is valid.

Drawing and navigation use local 4096-byte caches. They decode UTF-8 across
cache boundaries and do not allocate a complete line, including for a
single line spanning several gigabytes. The usual tab stops, wide and
combining characters, selection and sanitising of control bytes apply.
A failed source read stops traversal/drawing at the unavailable fragment;
failed reads of removed/copied bytes refuse the edit/copy. Applications can
record I/O errors in their source context and present them to the user.

## Memory and performance limits

This interface removes the requirement to keep a second complete document
inside the textarea. The provider decides which file blocks stay in RAM.
It does not make every operation constant-time or every allocation bounded:

- Vertical moves use line metadata. Computing a column, horizontal scrolling
  and hit testing scan the needed prefix of a line, using bounded memory.
- Undo/redo retain the bytes explicitly added or removed. History is bounded
  to `CTUI_TEXTAREA_HISTORY` records, rather than a fixed byte budget.
- Copying a selection allocates that selection in the internal clipboard;
  replacing the entire document explicitly still involves its full input.

For predictable RAM use with large files, bound the provider cache and
control large edits and clipboard operations in the application. This API
is a storage integration point; it does not ship a file-backed piece table.

Existing owned-buffer calls and pointer getter behaviour remain unchanged.
The public structs have grown, so rebuild applications against the new
headers and library together.
