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


def main(argv):
    if PIVOTAL_TOKEN_ENV_VAR not in os.environ:
        raise KeyError('%s not in environment' % PIVOTAL_TOKEN_ENV_VAR)

    token = os.environ[PIVOTAL_TOKEN_ENV_VAR]
    client = pivotal.PivotalClient(token=token, cache='.cache')
    projects = client.projects.all()['projects']

    wintox = None
    for project in projects:
        if project['name'].lower().strip() == 'wintox':
            wintox = project['id']
            break

    if wintox is None:
        raise CouldNotFindWintoxError('Could not find Wintox in projects list.')

    all_stories = client.stories.all(wintox)['stories']
    stories = dict([(name, list(stories)) for name,stories in groupby(all_stories, lambda story: story['current_state'])])
    blocks = {}
    for group,statuses in PIVOTAL_STATUS_GROUPS.iteritems():
        if len(statuses) == 1:
            blocks[group] = stories.get(statuses[0], [])
            continue

        block = []
        for status in statuses:
            block += stories.get(status, [])

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

