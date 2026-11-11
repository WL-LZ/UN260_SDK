"""Native source list shared by the update-engine test programs."""
from pathlib import Path

def storage_test_sources(source):
    tools = Path(source).parent
    return [str(tools / 'un260_history_space.c'),
            str(tools.parent / 'un260/history/history_object_store.c')]
