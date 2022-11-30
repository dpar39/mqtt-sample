import re
import os
import time
import shlex
import signal
import unittest
from subprocess import PIPE, Popen

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
MQTT_CLIENT_APP = os.path.join(THIS_DIR, "../build_release/mqtt-client-app")
MQTT_CLIENT_APP = os.getenv("MQTT_CLIENT_APP", MQTT_CLIENT_APP)


class Process:
    def __init__(self, cmd, **kwargs):
        if isinstance(cmd, str):
            cmd = shlex.split(cmd)
        self.process = Popen(cmd, stdout=PIPE, stderr=PIPE, **kwargs)

    def wait_for_output(self, re_pattern, stdout=True, log_lines=False):
        std_stream = self.process.stdout if stdout else self.process.stderr
        for line in iter(std_stream.readline, ""):
            line_str = line.decode()
            if log_lines:
                print(line_str, end="")
            if re.search(re_pattern, line_str):
                return

    def interrupt(self, timeout=None):
        self.process.send_signal(signal.SIGINT)
        return self.wait_for_completion(timeout)

    def stdout_str(self):
        return self.process.stdout.read().decode()

    def stderr_str(self):
        return self.process.stderr.read().decode()

    def wait_for_completion(self, timeout=None):
        rc = self.process.wait(timeout)
        out = self.process.stdout.read()
        err = self.process.stderr.read()
        self.process.stdout.close()
        self.process.stderr.close()
        return rc, out, err

    def __del__(self):
        if self.process.poll() is None:
            self.interrupt()


def config_mosquitto():
    mosquitto_conf = """

    """
    config_file = os.path.join(THIS_DIR, "mosquitto.conf")
    with open(config_file, "w") as fp:
        fp.write(mosquitto_conf)
    return config_file


def make_app_env(
    publish_topic,
    consume_topic,
    num_messages_to_send=1,
    hostname="localhost",
    port=1883,
):
    env = os.environ.copy()
    env.update(
        {
            "IO_HOST": f"{hostname}",
            "IO_PORT": f"{port}",
            "IO_PUBLISH_TOPIC": f"{publish_topic}",
            "IO_CONSUME_TOPIC": f"{consume_topic}",
            "IO_MESSAGE_COUNT": f"{num_messages_to_send}",
            "IO_MESSAGE_PERIOD_SECONDS": "0.01",
        }
    )
    return env


class Testing(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        config_file = config_mosquitto()
        cls.broker_process = Process(f"mosquitto -c {config_file}")
        broker_ready = re.compile(r"mosquitto version \d+\.\d+\.\d+ running")
        cls.broker_process.wait_for_output(broker_ready, False)

    @classmethod
    def tearDownClass(cls):
        cls.broker_process.interrupt()

    def test_can_published_messages_to_broker(self):

        publish_topic_name = "publish_feed"
        consume_topic_name = "consume_feed"
        num_messages_to_send = 3

        env = make_app_env(publish_topic_name, consume_topic_name, num_messages_to_send)
        mosquitto_sub = Process(f"mosquitto_sub -t {publish_topic_name} -h localhost")
        time.sleep(0.1)

        app_process = Process(MQTT_CLIENT_APP, env=env)
        rc, out, _ = app_process.wait_for_completion()
        self.assertEqual(0, rc)

        re_msg = re.compile(r"Publishing message: (.+)")
        publish_matches = map(lambda l: re.match(re_msg, l), out.decode().splitlines())
        sent_messages = list(m.group(1) for m in publish_matches if m)

        time.sleep(0.1)
        _, out, _ = mosquitto_sub.interrupt()
        received_messages = out.decode().splitlines(keepends=False)

        # Check messages sent by the app and the ones received by mosquitto_sub matches
        self.assertEqual(
            num_messages_to_send,
            len(sent_messages),
            "Number of messages sent and received should match!",
        )
        self.assertEqual(sent_messages, received_messages)

    def test_can_receive_messages_from_broker(self):
        publish_topic_name = "publish_feed"
        consume_topic_name = "consume_feed"
        num_messages_to_send = -1
        sample_message = "Message from Mars"

        # Star the mqtt-client-app and wait until it is subscribed to the broker
        env = make_app_env(publish_topic_name, consume_topic_name, num_messages_to_send)
        app_process = Process(MQTT_CLIENT_APP, env=env)
        app_process.wait_for_output("on_subscribe: ")

        # Send a message to the broker
        mosquitto_pub = Process(
            f"mosquitto_pub -t {consume_topic_name} -h localhost -m '{sample_message}'"
        )
        mosquitto_pub.wait_for_completion()
        time.sleep(0.1)

        rc, out, _ = app_process.interrupt()
        self.assertEqual(0, rc, "client app should return code 0 even when interrupted")

        # Check the message was received
        self.assertIn(sample_message, out.decode())


if __name__ == "__main__":
    unittest.main()
