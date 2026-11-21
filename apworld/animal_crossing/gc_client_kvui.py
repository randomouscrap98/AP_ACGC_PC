# The GC client's Items and Checks tabs in Archipelago's kivy client (kvui): ACGCContext.make_gui() returns
# manager_class(). Only imported from there: kivy isn't installed under aplite (a module-level kvui import breaks
# the client there). Rows come from gc_client_lists.py; each tab is a search row over a flat read-only list
# (kvui's log list: tapping a row copies it).
from typing import Any

from kivy.clock import Clock
from kivy.lang import Builder
from kivy.metrics import dp
from kivy.utils import escape_markup
from kivymd.uix.boxlayout import MDBoxLayout
from kivymd.uix.label import MDLabel
from kivymd.uix.selectioncontrol import MDCheckbox

from kvui import GameManager, ResizableTextField, UILog

from . import gc_client_lists as lists

REFRESH_SECONDS = 0.5


class ACGCSearchField(ResizableTextField):
    pass


class ACGCList(UILog):
    pass


# ResizableTextField takes its size from a kv rule (kvui.py); list starts at the top, not kvui's log bottom
Builder.load_string("""
<ACGCSearchField>:
    size_hint_y: None
    height: "30dp"
    multiline: False
    write_tab: False
    role: "medium"
    pos_hint: {"center_x": 0.5, "center_y": 0.5}
<ACGCList>:
    scroll_y: 1
""")


def colored(text: str, hex_rgb: str) -> str:
    return f"[color={hex_rgb}]{escape_markup(text)}[/color]"


class ListTab(MDBoxLayout):
    # Search row (+ extra widgets), optional summary line, list; refresh() rebuilds the list when its key changes
    def __init__(self, ctx: Any, summary: bool, **kwargs: Any) -> None:
        super().__init__(orientation="vertical", **kwargs)
        self.ctx = ctx
        self.shown: Any = None
        self.row = MDBoxLayout(orientation="horizontal", size_hint_y=None, height=dp(40), spacing=dp(6),
                               padding=(dp(6), 0))
        self.row.add_widget(MDLabel(text="Search:", size_hint_x=None, width=dp(60), halign="center"))
        self.search = ACGCSearchField()
        self.search.bind(text=lambda *_: self.refresh())
        self.row.add_widget(self.search)
        self.add_widget(self.row)
        self.summary = None
        if summary:
            self.summary = MDLabel(size_hint_y=None, height=dp(24), padding=(dp(8), 0))
            self.add_widget(self.summary)
        self.list = ACGCList()
        self.add_widget(self.list)
        Clock.schedule_interval(lambda _: self.refresh(), REFRESH_SECONDS)

    def fix_heights(self) -> None:  # GameManager calls it on the current tab's content
        self.list.fix_heights()

    def refresh(self) -> None:
        raise NotImplementedError


class ItemsTab(ListTab):
    def __init__(self, ctx: Any) -> None:
        super().__init__(ctx, summary=False)
        self.refresh()

    def refresh(self) -> None:
        query = self.search.text.strip()
        key = (lists.version(self.ctx), query)
        if key == self.shown:
            return
        self.shown = key
        data = []
        for r in lists.item_rows(self.ctx):
            if not lists.item_matches(r, query):
                continue
            where = colored(r.location, lists.COLOR_LOCATION if r.has_location else lists.COLOR_DONE)
            data.append({"text": f"{r.number}. {colored(r.item, lists.item_color(r.flags))} from "
                                 f"{escape_markup(r.sender)} ({where})"})
        if not data:
            data.append({"text": colored("No items received yet." if not query else "No matches.",
                                         lists.COLOR_DONE)})
        self.list.data = data


class ChecksTab(ListTab):
    def __init__(self, ctx: Any) -> None:
        super().__init__(ctx, summary=True)
        self.show_done = MDCheckbox(size_hint=(None, None), size=(dp(36), dp(36)),
                                    pos_hint={"center_y": 0.5})
        self.show_done.bind(active=lambda *_: self.refresh())
        self.row.add_widget(self.show_done)
        self.row.add_widget(MDLabel(text="Show checked", size_hint_x=None, width=dp(110)))
        self.refresh()

    def refresh(self) -> None:
        query = self.search.text.strip()
        show_done = self.show_done.active
        key = (lists.version(self.ctx), query, show_done)
        if key == self.shown:
            return
        self.shown = key
        groups = lists.check_groups(self.ctx)
        self.summary.text = lists.summary(groups)
        data = []
        for g in groups:
            shown = [c for c in g.checks if lists.check_shown(c, query, show_done)]
            if not shown:
                continue
            # Flat list: a bold header row per group, then its checks indented
            data.append({"text": f"[b]{escape_markup(g.name)}[/b]   "
                                 f"{colored(f'{g.missing} left of {len(g.checks)}', lists.COLOR_DONE)}"})
            for c in shown:
                # Checked: gray only (kivy's font may lack a check mark)
                data.append({"text": "      " + colored(c.name, lists.COLOR_DONE if c.checked else lists.COLOR_LOCATION)})
        self.list.data = data


def manager_class() -> type:
    class ACGCManager(GameManager):
        base_title = "Animal Crossing GC Client"

        def build(self) -> Any:
            container = super().build()
            self.add_client_tab("Items", ItemsTab(self.ctx))
            self.add_client_tab("Checks", ChecksTab(self.ctx))
            return container

    return ACGCManager
