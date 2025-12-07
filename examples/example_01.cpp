import zephyr.application;
import zephyr.dummyPlugin;

using App = zephyr::Application<
    zephyr::DummyPlugin
>;

int main()
{
    App app;

    app.start();

    return 0;
}
