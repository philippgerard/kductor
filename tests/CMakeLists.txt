ecm_add_test(agentoutputmodeltest.cpp
    TEST_NAME AgentOutputModelTest
    LINK_LIBRARIES Qt6::Test kductor_core
)

ecm_add_test(workspacetest.cpp
    TEST_NAME WorkspaceTest
    LINK_LIBRARIES Qt6::Test kductor_core
)

ecm_add_test(gitmanagertest.cpp
    TEST_NAME GitManagerTest
    LINK_LIBRARIES Qt6::Test kductor_core
)

ecm_add_test(agentprocesstest.cpp
    TEST_NAME AgentProcessTest
    LINK_LIBRARIES Qt6::Test kductor_core
)

ecm_add_test(agentmanagertest.cpp
    TEST_NAME AgentManagerTest
    LINK_LIBRARIES Qt6::Test kductor_core
)

ecm_add_test(workspacemodeltest.cpp
    TEST_NAME WorkspaceModelTest
    LINK_LIBRARIES Qt6::Test kductor_core
)

ecm_add_test(worktreemanagertest.cpp
    TEST_NAME WorktreeManagerTest
    LINK_LIBRARIES Qt6::Test kductor_core
)
