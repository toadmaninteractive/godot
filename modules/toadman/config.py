def can_build(env, platform):
    if platform == 'ps4':
        return False

    return env['ps4_tools']

def configure(env):
    pass
