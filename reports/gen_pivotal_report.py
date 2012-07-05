import pytz
import os
from datetime import datetime
from jinja2 import Template
from itertools import groupby
from busyflow import pivotal

PIVOTAL_TOKEN_ENV_VAR = 'PIVOTAL_TOKEN'
PIVOTAL_STATUS_GROUPS = {
    'unstarted': ('unscheduled', 'unstarted', 'rejected'),
    'started': ('started',),
    'finished': ('finished'),
    'delivered': ('delivered',),
    'accepted': ('accepted',),
}
REPORT_BLOCK_VERBOSE = {
    'unstarted': 'To be started',
    'started': 'Work in progress',
    'finished': 'Finished, but not delivered',
    'delivered': 'Finished and waiting for customer approval',
    'accepted': 'Completed'
}
REPORT_BLOCKS_ORDER = ('delivered', 'finished', 'started', 'unstarted', 'accepted')


class CouldNotFindWintoxError(Exception):
    pass


def correct_times(block):
    # Grab our timezone objects
    tz_utc = pytz.timezone('UTC')
    tz_eastern = pytz.timezone('America/New_York')

    for story in block:
        for key in ('created_at', 'updated_at'):
            story[key] = story[key].replace(tzinfo=tz_utc)
            story[key] = story[key].astimezone(tz_eastern)

    return block


def main(argv):
    if PIVOTAL_TOKEN_ENV_VAR not in os.environ:
        raise KeyError('%s not in environment' % PIVOTAL_TOKEN_ENV_VAR)

    # Log ourselves in using our API token from the env var
    token = os.environ[PIVOTAL_TOKEN_ENV_VAR]
    client = pivotal.PivotalClient(token=token, cache='.cache')

    wintox = None
    for project in client.projects.all()['projects']:
        if project['name'].lower().strip() == 'wintox':
            wintox = project['id']
            break

    if wintox is None:
        raise CouldNotFindWintoxError('Could not find Wintox in projects list.')

    all_stories = client.stories.all(wintox)['stories']
    # Creates a dict of { state: [story, story2, ...] }
    stories = dict([(name, list(stories)) for name,stories in groupby(all_stories, lambda story: story['current_state'])])

    blocks = {}
    for group,statuses in PIVOTAL_STATUS_GROUPS.iteritems():
        if len(statuses) == 1:
            blocks[group] = correct_times(stories.get(statuses[0], []))
            continue

        block = []
        for status in statuses:
            block += stories.get(status, [])

        block = correct_times(block)

        sorted(block, key=lambda story: story['created_at'], reverse=True)
        blocks[group] = block

    context = {
        'now': datetime.now().strftime('%c'),
        'stories': blocks,
        'BLOCKS': [(block,REPORT_BLOCK_VERBOSE[block]) for block in REPORT_BLOCKS_ORDER]
    }

    with open('_report.htm', 'r') as fp:
        tmpl = Template(fp.read())
    with open('report.htm', 'w') as fp:
        fp.write(tmpl.render(**context))
        print 'Successfully wrote %d bytes to %s!' % (fp.tell(), fp.name)


if __name__ == '__main__':
    import sys
    main(sys.argv)

