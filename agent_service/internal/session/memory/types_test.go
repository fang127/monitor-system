package memory

import "testing"

func TestMemoryScope_SessionKey(t *testing.T) {
	tests := []struct {
		name string
		want string
	}{
		{
			name: "test case 1",
			want: "tenant-id-01/team-id-01/cluster-id-01/session-id-01",
		},
		{
			name: "test case 2",
			want: "tenant-id-02/team-id-02/cluster-id-02/session-id-02",
		},
	}
	s := []MemoryScope{
		{TenantID: "tenant-id-01",
			TeamID:    "team-id-01",
			ClusterID: "cluster-id-01",
			SessionID: "session-id-01"},
		{
			TenantID:  "tenant-id-02",
			TeamID:    "team-id-02",
			ClusterID: "cluster-id-02",
			SessionID: "session-id-02",
		},
	}

	for i, tt := range tests {
		if got := s[i].SessionKey(); got != tt.want {
			t.Errorf("%q. MemoryScope.SessionKey() = %v, want %v", tt.name, got, tt.want)
		} else {
			t.Logf("%q. MemoryScope.SessionKey() = %v, as expected", tt.name, got)
		}
	}
}

func TestMemoryScopeMostSpecificPolicyLevelWithoutDefaults(t *testing.T) {
	tests := []struct {
		name  string
		scope MemoryScope
		want  ScopeLevel
	}{
		{name: "空作用域使用全局层级", scope: MemoryScope{}, want: ScopeLevelGlobal},
		{name: "只有租户使用租户层级", scope: MemoryScope{TenantID: "tenant-a"}, want: ScopeLevelTenant},
		{name: "租户团队使用团队层级", scope: MemoryScope{TenantID: "tenant-a", TeamID: "team-a"}, want: ScopeLevelTeam},
		{name: "带集群使用集群层级", scope: MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a"}, want: ScopeLevelCluster},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := tt.scope.MostSpecificPolicyLevel(); got != tt.want {
				t.Fatalf("MostSpecificPolicyLevel() = %s, want %s", got, tt.want)
			}
		})
	}
}
