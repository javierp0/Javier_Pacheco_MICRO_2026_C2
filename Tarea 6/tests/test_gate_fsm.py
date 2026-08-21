import unittest
from dataclasses import dataclass


class State:
    STARTING = "INICIANDO"
    STOPPED = "DETENIDO"
    OPENING = "ABRIENDO"
    CLOSING = "CERRANDO"
    OPEN = "ABIERTO"
    CLOSED = "CERRADO"
    WAITING_AUTO_CLOSE = "ESPERANDO_AUTO_CIERRE"
    PAUSED_BY_FTC = "PAUSADO_POR_FTC"
    ERROR = "ERROR"
    CALIBRATING = "CALIBRANDO"


class Error:
    NONE = "NONE"
    BOTH_LIMITS = "BOTH_LIMITS"
    TRAVEL_TIMEOUT = "TRAVEL_TIMEOUT"
    MAINTENANCE_LOCK = "MAINTENANCE_LOCK"


@dataclass
class Inputs:
    limit_open: bool = False
    limit_closed: bool = False
    ftc_blocked: bool = False
    dip_ftc_reverse: bool = False
    dip_auto_close: bool = False
    dip_maintenance: bool = False


@dataclass
class Config:
    auto_close_s: int = 3
    max_travel_s: int = 5
    ftc_wait_s: int = 2
    reverse_pause_s: int = 1
    auto_close_sw: bool = True
    maintenance_sw: bool = False


class GateModel:
    """Modelo minimo para validar los escenarios esperados de la FSM."""

    def __init__(self, config=None):
        self.config = config or Config()
        self.state = State.STARTING
        self.error = Error.NONE
        self.time_s = 0
        self.enter_s = 0
        self.movement_s = 0

    def boot(self, inputs):
        if inputs.limit_open and inputs.limit_closed:
            self._error(Error.BOTH_LIMITS)
        elif inputs.limit_open:
            self._go(State.OPEN)
        elif inputs.limit_closed:
            self._go(State.CLOSED)
        else:
            self._go(State.STOPPED)

    def dispatch(self, event, inputs=None):
        inputs = inputs or Inputs()
        if inputs.limit_open and inputs.limit_closed:
            self._error(Error.BOTH_LIMITS)
            return False
        if event == "STOP":
            self._go(State.STOPPED)
            return True
        if event == "RESET_ERROR" and self.state == State.ERROR:
            self.error = Error.NONE
            self._go(State.STOPPED)
            return True
        if event == "OPEN":
            if self._maintenance(inputs):
                self._error(Error.MAINTENANCE_LOCK)
                return False
            self._go(State.OPENING, moving=True)
            return True
        if event == "CLOSE":
            if self._maintenance(inputs):
                self._error(Error.MAINTENANCE_LOCK)
                return False
            self._go(State.CLOSING, moving=True)
            return True
        return False

    def tick(self, seconds, inputs=None):
        inputs = inputs or Inputs()
        self.time_s += seconds

        if inputs.limit_open and inputs.limit_closed:
            self._error(Error.BOTH_LIMITS)
            return

        if self.state == State.OPENING:
            if inputs.limit_open:
                self._go(State.OPEN)
            elif self.time_s - self.movement_s >= self.config.max_travel_s:
                self._error(Error.TRAVEL_TIMEOUT)

        if self.state == State.CLOSING:
            if inputs.ftc_blocked:
                self._go(State.PAUSED_BY_FTC)
            elif inputs.limit_closed:
                self._go(State.CLOSED)
            elif self.time_s - self.movement_s >= self.config.max_travel_s:
                self._error(Error.TRAVEL_TIMEOUT)

        if self.state == State.OPEN:
            if self.config.auto_close_sw and inputs.dip_auto_close and not self._maintenance(inputs):
                self._go(State.WAITING_AUTO_CLOSE)

        if self.state == State.WAITING_AUTO_CLOSE:
            if self._maintenance(inputs):
                self._go(State.STOPPED)
            elif self.time_s - self.enter_s >= self.config.auto_close_s:
                self._go(State.CLOSING, moving=True)

        if self.state == State.PAUSED_BY_FTC:
            if self.time_s - self.enter_s >= self.config.ftc_wait_s and not inputs.ftc_blocked:
                if inputs.dip_ftc_reverse:
                    self._go(State.OPENING, moving=True)
                else:
                    self._go(State.CLOSING, moving=True)

    def _maintenance(self, inputs):
        return inputs.dip_maintenance or self.config.maintenance_sw

    def _go(self, state, moving=False):
        self.state = state
        self.enter_s = self.time_s
        if moving:
            self.movement_s = self.time_s

    def _error(self, error):
        self.error = error
        self._go(State.ERROR)


class GateFsmScenarios(unittest.TestCase):
    def test_open_until_open_limit(self):
        gate = GateModel()
        gate.boot(Inputs(limit_closed=True))
        gate.dispatch("OPEN")
        gate.tick(1, Inputs(limit_open=True))
        self.assertEqual(gate.state, State.OPEN)

    def test_close_until_closed_limit(self):
        gate = GateModel()
        gate.boot(Inputs(limit_open=True))
        gate.dispatch("CLOSE")
        gate.tick(1, Inputs(limit_closed=True))
        self.assertEqual(gate.state, State.CLOSED)

    def test_stop_while_moving(self):
        gate = GateModel()
        gate.boot(Inputs())
        gate.dispatch("OPEN")
        gate.dispatch("STOP")
        self.assertEqual(gate.state, State.STOPPED)

    def test_ftc_during_close_pauses_and_reverses_when_dip2_is_active(self):
        gate = GateModel()
        gate.boot(Inputs(limit_open=True))
        gate.dispatch("CLOSE")
        gate.tick(1, Inputs(ftc_blocked=True, dip_ftc_reverse=True))
        self.assertEqual(gate.state, State.PAUSED_BY_FTC)
        gate.tick(2, Inputs(dip_ftc_reverse=True))
        self.assertEqual(gate.state, State.OPENING)

    def test_travel_timeout_goes_to_error(self):
        gate = GateModel(Config(max_travel_s=2))
        gate.boot(Inputs())
        gate.dispatch("OPEN")
        gate.tick(2, Inputs())
        self.assertEqual(gate.state, State.ERROR)
        self.assertEqual(gate.error, Error.TRAVEL_TIMEOUT)

    def test_reset_error_returns_to_stopped(self):
        gate = GateModel(Config(max_travel_s=1))
        gate.boot(Inputs())
        gate.dispatch("OPEN")
        gate.tick(1)
        gate.dispatch("RESET_ERROR")
        self.assertEqual(gate.state, State.STOPPED)
        self.assertEqual(gate.error, Error.NONE)

    def test_auto_close(self):
        gate = GateModel(Config(auto_close_s=3))
        gate.boot(Inputs(limit_open=True))
        gate.tick(0, Inputs(dip_auto_close=True))
        self.assertEqual(gate.state, State.WAITING_AUTO_CLOSE)
        gate.tick(3, Inputs(dip_auto_close=True))
        self.assertEqual(gate.state, State.CLOSING)

    def test_maintenance_dip_blocks_motion(self):
        gate = GateModel()
        gate.boot(Inputs())
        accepted = gate.dispatch("OPEN", Inputs(dip_maintenance=True))
        self.assertFalse(accepted)
        self.assertEqual(gate.state, State.ERROR)
        self.assertEqual(gate.error, Error.MAINTENANCE_LOCK)


if __name__ == "__main__":
    unittest.main(verbosity=2)
