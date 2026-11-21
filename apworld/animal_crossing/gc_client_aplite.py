# The GC client's Items and Checks tabs in aplite (ArchipelagoLite's terminal client): Textual panes returned by
# ACGCContext.aplite_tabs(), which aplite's client screen calls once (aplite/tui.py, ClientApp.compose). Only
# imported from there: Textual exists only under aplite. Rows come from gc_client_lists.py.
from typing import Any

from rich.text import Text
from textual.app import ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import Checkbox, DataTable, Input, Static, Tree

from . import gc_client_lists as lists

REFRESH_SECONDS = 0.5  # as aplite's own panes


def color(hex_rgb: str) -> str:
    return "#" + hex_rgb


class ItemsPane(Vertical):
    DEFAULT_CSS = """
    ItemsPane { height: 1fr; }
    ItemsPane #acgc-items-search { height: 1; }
    ItemsPane DataTable { height: 1fr; }
    """

    def __init__(self, ctx: Any) -> None:
        super().__init__()
        self.ctx = ctx
        self.shown: tuple | None = None  # (version, query) the table is from

    def compose(self) -> ComposeResult:
        # tab-search: aplite's Ctrl+F focuses it
        yield Input(placeholder="Search items, senders, locations", id="acgc-items-search", classes="tab-search",
                    compact=True)
        yield DataTable(id="acgc-items", cursor_type="row", zebra_stripes=True)

    def on_mount(self) -> None:
        self.query_one(DataTable).add_columns("#", "Item", "From", "Location")
        self.set_interval(REFRESH_SECONDS, self.refresh_rows)
        self.refresh_rows()

    def on_input_changed(self, event: Input.Changed) -> None:
        event.stop()
        self.refresh_rows()

    def refresh_rows(self) -> None:
        query = self.query_one(Input).value.strip()
        key = (lists.version(self.ctx), query)
        if key == self.shown:
            return
        self.shown = key
        table = self.query_one(DataTable)
        table.clear()
        for r in lists.item_rows(self.ctx):
            if not lists.item_matches(r, query):
                continue
            table.add_row(Text(str(r.number), style="dim"),
                          Text(r.item, style=color(lists.item_color(r.flags))),
                          Text(r.sender, style="dim" if not r.has_location else ""),
                          Text(r.location, style=color(lists.COLOR_LOCATION) if r.has_location else "dim"))


class ChecksPane(Vertical):
    DEFAULT_CSS = """
    ChecksPane { height: 1fr; }
    ChecksPane Horizontal { height: 1; }
    ChecksPane #acgc-checks-search { width: 1fr; }
    ChecksPane Checkbox { width: auto; height: 1; border: none; padding: 0 1; }
    ChecksPane #acgc-checks-summary { height: 1; padding: 0 1; }
    ChecksPane Tree { height: 1fr; }
    """

    def __init__(self, ctx: Any) -> None:
        super().__init__()
        self.ctx = ctx
        self.shown: tuple | None = None  # (version, query, show checked) the tree is from

    def compose(self) -> ComposeResult:
        with Horizontal():
            yield Input(placeholder="Search checks", id="acgc-checks-search", classes="tab-search", compact=True)
            yield Checkbox("Show checked", id="acgc-checks-done", compact=True)
        yield Static(id="acgc-checks-summary")
        yield Tree("Checks", id="acgc-checks")

    def on_mount(self) -> None:
        self.query_one(Tree).show_root = False
        self.set_interval(REFRESH_SECONDS, self.refresh_tree)
        self.refresh_tree()

    def on_input_changed(self, event: Input.Changed) -> None:
        event.stop()
        self.refresh_tree()

    def on_checkbox_changed(self, event: Checkbox.Changed) -> None:
        event.stop()
        self.refresh_tree()

    def refresh_tree(self) -> None:
        query = self.query_one(Input).value.strip()
        show_done = self.query_one(Checkbox).value
        key = (lists.version(self.ctx), query, show_done)
        if key == self.shown:
            return
        self.shown = key
        groups = lists.check_groups(self.ctx)
        self.query_one("#acgc-checks-summary", Static).update(lists.summary(groups))
        tree = self.query_one(Tree)
        # Rebuilt from scratch: keep which groups were open (searching opens every group with a match)
        expanded = {n.data for n in tree.root.children if n.is_expanded}
        tree.clear()
        for g in groups:
            shown = [c for c in g.checks if lists.check_shown(c, query, show_done)]
            if not shown:
                continue
            label = Text(g.name)
            label.append(f"   {g.missing} left of {len(g.checks)}", style="dim")
            node = tree.root.add(label, data=g.name, expand=bool(query) or g.name in expanded)
            for c in shown:
                if c.checked:
                    node.add_leaf(Text("✓ " + c.name, style=color(lists.COLOR_DONE)))
                else:
                    node.add_leaf(Text(c.name, style=color(lists.COLOR_LOCATION)))


def tabs(ctx: Any) -> list[tuple[str, Any]]:
    return [("Items", ItemsPane(ctx)), ("Checks", ChecksPane(ctx))]
