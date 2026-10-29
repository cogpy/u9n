#include <gtest/gtest.h>
#include <stdexcept>
#include "deep_tree_echo/core.hpp"
#include "deep_tree_echo/unreal_integration.hpp"

using namespace deep_tree_echo;

TEST(AvatarInterfaceTest, RegisteredToolRuns) {
    AvatarInterface avatar;
    avatar.register_tool("double", [](const Vector& v) { return Vector(v * 2.0); });
    const Vector out = avatar.use_tool("double", Vector::Ones(3));
    EXPECT_DOUBLE_EQ(out(0), 2.0);
    EXPECT_EQ(out.size(), 3);
}

TEST(AvatarInterfaceTest, UnregisteredToolThrowsInsteadOfEchoingInput) {
    AvatarInterface avatar;
    EXPECT_THROW(avatar.use_tool("missing", Vector::Ones(3)), std::out_of_range);
}

namespace {
std::shared_ptr<MembraneLayer> make_core(int reservoir_size) {
    EchoConfig config;
    config.reservoir_size = reservoir_size;
    config.seed = 7;
    auto echo = std::make_shared<EchoState>(config);
    echo->initialize();
    auto node = std::make_shared<CognitiveNode>("motor", MembraneType::ROOT);
    node->set_echo_state(echo);
    auto core = std::make_shared<MembraneLayer>("root", MembraneType::ROOT);
    core->add_node(node);
    return core;
}
}  // namespace

TEST(AvatarInterfaceTest, UntrainedCognitionProducesNoMotorOutput) {
    AvatarInterface avatar;
    avatar.set_cognitive_core(make_core(20));
    avatar.receive_proprioception(Vector::Ones(6));
    avatar.process_cycle();
    EXPECT_FALSE(avatar.has_trained_motor_readout());
    EXPECT_TRUE(avatar.generate_movement().isZero());
    EXPECT_TRUE(avatar.generate_expression().isZero());
    EXPECT_TRUE(avatar.generate_speech_params().isZero());

    // An untrained readout is no better than none
    avatar.set_motor_readout(std::make_shared<LinearReadout>());
    avatar.process_cycle();
    EXPECT_TRUE(avatar.generate_movement().isZero());
    EXPECT_TRUE(avatar.generate_expression().isZero());
}

TEST(AvatarInterfaceTest, TrainedReadoutDrivesMotorOutput) {
    const int n = 20;
    auto core = make_core(n);
    AvatarInterface avatar;
    avatar.set_cognitive_core(core);

    // Fit a readout that maps any cognitive state to a known motor pattern
    Matrix states = Matrix::Random(60, n);
    Matrix targets = Matrix::Zero(60, 24);
    for (int r = 0; r < 60; ++r) {
        targets(r, 0) = states.row(r).sum();
    }
    auto readout = std::make_shared<LinearReadout>();
    readout->fit(states, targets);
    avatar.set_motor_readout(readout);
    ASSERT_TRUE(avatar.has_trained_motor_readout());

    avatar.receive_proprioception(Vector::Ones(6));
    avatar.process_cycle();
    const Vector state = core->get_aggregate_output();
    EXPECT_NEAR(avatar.generate_movement()(0), state.sum(), 1e-3);
}

TEST(LinearReadoutTest, UntrainedReadoutThrows) {
    LinearReadout readout;
    EXPECT_FALSE(readout.is_trained());
    EXPECT_THROW(readout.apply(Vector::Ones(3)), std::runtime_error);
}

TEST(UnrealAvatarTest, UntrainedCognitionMovesNothing) {
    EchoConfig config;
    config.reservoir_size = 30;
    config.seed = 3;
    auto avatar = unreal::create_avatar(config);
    const Vector movement_before = avatar->character().get_proprioception();
    const Vector face_before = avatar->face().get_expression_state();
    avatar->tick(0.016);
    avatar->tick(0.016);
    EXPECT_FALSE(avatar->has_trained_action_readout());
    EXPECT_TRUE(avatar->character().get_proprioception().isApprox(movement_before) ||
                (avatar->character().get_proprioception() - movement_before).isZero());
    EXPECT_TRUE((avatar->face().get_expression_state() - face_before).isZero());
}
